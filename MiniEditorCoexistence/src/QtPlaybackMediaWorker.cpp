#include "QtPlaybackMediaWorker.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

#include <utility>

using mini_editor::playback_core::DecodedVideoFrame;
using mini_editor::playback_core::FramePresentationRequest;
using mini_editor::playback_core::IVideoCompositor;
using mini_editor::playback_core::IVideoDecodeService;
using mini_editor::playback_core::PresentedPosition;
using mini_editor::playback_core::PresentedSequencePosition;
using mini_editor::playback_core::PresentedSourcePosition;
using mini_editor::playback_core::SequencePresentationTarget;
using mini_editor::playback_core::SourcePresentationTarget;
using mini_editor::playback_core::SourceTimestamp;
using mini_editor::playback_core::VideoDecodeRequest;
using mini_editor::playback_core::VideoFrameBuffer;

namespace {

VideoFrameBuffer wrapFrame(const QVideoFrame &frame)
{
    VideoFrameBuffer buffer;
    buffer.placeholderPixelChecksum = frame.isValid() ? (frame.width() * 31 + frame.height()) : 0;
    buffer.platformHandle = std::make_shared<QVideoFrame>(frame);
    return buffer;
}

} // namespace

// Implements IVideoDecodeService by marshaling onto the worker thread: pause
// (if needed), seek, and report back the frame QVideoSink holds once the
// seek settles. This is the least certain part of this adapter -- exactly
// what the manual smoke test's "seek accuracy" step exists to check.
class QtPlaybackMediaWorker::VideoDecodeServiceImpl final : public IVideoDecodeService {
public:
    explicit VideoDecodeServiceImpl(QtPlaybackMediaWorker &owner) : owner_(owner) {}

    void requestDecode(VideoDecodeRequest request,
                       std::function<void(DecodedVideoFrame)> onDecoded) override
    {
        QMetaObject::invokeMethod(
            &owner_,
            [this, request = std::move(request), onDecoded = std::move(onDecoded)]() mutable {
                owner_.ensureMediaObjectsExist();
                if (owner_.player_->playbackState() != QMediaPlayer::PausedState)
                    owner_.player_->pause();

                const qint64 targetMs = request.sourceTime.microsecondsForAdapter() / 1000;
                auto deliver = [this, request, onDecoded]() mutable {
                    onDecoded(DecodedVideoFrame{
                        request.sequence, request.mediaAssetId, request.sourceTime,
                        wrapFrame(owner_.videoSink_->videoFrame())
                    });
                };

                // Deliver once the seek settles, or after a bounded fallback
                // delay so a VideoWorkScheduler request is never left
                // waiting forever if positionChanged doesn't fire as
                // expected for this source.
                auto connection = std::make_shared<QMetaObject::Connection>();
                auto delivered = std::make_shared<bool>(false);
                *connection = QObject::connect(
                    owner_.player_, &QMediaPlayer::positionChanged, &owner_,
                    [connection, delivered, deliver](qint64) mutable {
                        if (*delivered)
                            return;
                        *delivered = true;
                        QObject::disconnect(*connection);
                        deliver();
                    });
                QTimer::singleShot(200, &owner_, [connection, delivered, deliver]() mutable {
                    if (*delivered)
                        return;
                    *delivered = true;
                    QObject::disconnect(*connection);
                    deliver();
                });

                owner_.player_->setPosition(targetMs);
            },
            Qt::QueuedConnection);
    }

private:
    QtPlaybackMediaWorker &owner_;
};

// Milestone 1's compositor is a pass-through (target-architecture note:
// "the first implementation may use CPU image processing" -- applying
// timeline opacity/fade/effects here is real compositing work this
// milestone does not attempt; see the M4-04 issue).
class QtPlaybackMediaWorker::VideoCompositorImpl final : public IVideoCompositor {
public:
    void composite(DecodedVideoFrame frame, FramePresentationRequest request,
                   std::function<void(mini_editor::playback_core::CompositedVideoFrame)> onComposited) override
    {
        const PresentedPosition position = [&request]() -> PresentedPosition {
            if (const auto *source = std::get_if<SourcePresentationTarget>(&request.target)) {
                return PresentedPosition{PresentedSourcePosition{source->mediaAssetId, source->sourceTimestamp}};
            }
            const auto &sequence = std::get<SequencePresentationTarget>(request.target);
            return PresentedPosition{PresentedSequencePosition{
                sequence.sequenceId, sequence.sequenceRevision, sequence.timelineFrame}};
        }();

        onComposited(mini_editor::playback_core::CompositedVideoFrame{
            request.presentationSessionId, request.requestId, request.authority, position, frame.buffer
        });
    }
};

QtPlaybackMediaWorker::QtPlaybackMediaWorker()
    : videoDecodeService_(std::make_unique<VideoDecodeServiceImpl>(*this))
    , videoCompositor_(std::make_unique<VideoCompositorImpl>())
{
    // No parent: moveToThread() requires it, and ADR-005 says no engine
    // QObject uses a UI parent regardless.
    moveToThread(&thread_);
    thread_.start();
}

QtPlaybackMediaWorker::~QtPlaybackMediaWorker()
{
    thread_.quit();
    thread_.wait();
}

void QtPlaybackMediaWorker::ensureMediaObjectsExist()
{
    // Always called on the worker thread (either via QMetaObject::invokeMethod
    // targeting `this`, or directly from a method already running there).
    if (player_)
        return;

    audioOutput_ = new QAudioOutput(this);
    videoSink_ = externalVideoSink_ ? externalVideoSink_ : new QVideoSink(this);
    player_ = new QMediaPlayer(this);
    player_->setAudioOutput(audioOutput_);
    player_->setVideoOutput(videoSink_);

    QObject::connect(player_, &QMediaPlayer::errorOccurred, this,
                     [this](QMediaPlayer::Error, const QString &errorString) {
                         emit mediaErrorOccurred(errorString);
                     });
    QObject::connect(videoSink_, &QVideoSink::videoFrameChanged, this,
                     [this](const QVideoFrame &frame) {
                         if (player_->playbackState() == QMediaPlayer::PlayingState)
                             emit framePlayed(wrapFrame(frame));
                     });
}

void QtPlaybackMediaWorker::openSource(const QString &filePath)
{
    QMetaObject::invokeMethod(this, [this, filePath] {
        ensureMediaObjectsExist();
        player_->setSource(QUrl::fromLocalFile(filePath));
    }, Qt::QueuedConnection);
}

void QtPlaybackMediaWorker::play()
{
    QMetaObject::invokeMethod(this, [this] {
        ensureMediaObjectsExist();
        player_->play();
    }, Qt::QueuedConnection);
}

void QtPlaybackMediaWorker::pause()
{
    QMetaObject::invokeMethod(this, [this] {
        ensureMediaObjectsExist();
        player_->pause();
    }, Qt::QueuedConnection);
}

void QtPlaybackMediaWorker::stop()
{
    QMetaObject::invokeMethod(this, [this] {
        ensureMediaObjectsExist();
        player_->stop();
    }, Qt::QueuedConnection);
}

void QtPlaybackMediaWorker::seekTo(SourceTimestamp position)
{
    QMetaObject::invokeMethod(this, [this, position] {
        ensureMediaObjectsExist();
        player_->setPosition(position.microsecondsForAdapter() / 1000);
    }, Qt::QueuedConnection);
}

void QtPlaybackMediaWorker::setRatePercent(int ratePercent)
{
    QMetaObject::invokeMethod(this, [this, ratePercent] {
        ensureMediaObjectsExist();
        player_->setPlaybackRate(ratePercent / 100.0);
    }, Qt::QueuedConnection);
}

IVideoDecodeService &QtPlaybackMediaWorker::videoDecodeService()
{
    return *videoDecodeService_;
}

IVideoCompositor &QtPlaybackMediaWorker::videoCompositor()
{
    return *videoCompositor_;
}

void QtPlaybackMediaWorker::attachExternalVideoOutput(QVideoSink *sink)
{
    QMetaObject::invokeMethod(this, [this, sink] {
        externalVideoSink_ = sink;
    }, Qt::QueuedConnection);
}
