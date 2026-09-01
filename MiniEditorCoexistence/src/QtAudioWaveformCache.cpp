#include "QtAudioWaveformCache.h"

#include "MediaLibrary.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QQueue>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace {

constexpr int kSampleFramesPerPeak = 256;

class AudioWaveformDecoderWorker final : public QObject
{
public:
    using DecodedHandler = std::function<void(int, std::uint64_t,
                                               AudioWaveformData)>;

    AudioWaveformDecoderWorker(QObject *deliveryContext,
                               DecodedHandler decodedHandler)
        : deliveryContext_(deliveryContext),
          decodedHandler_(std::move(decodedHandler))
    {
    }

    void enqueue(int mediaAssetId, const QString &filePath,
                 std::uint64_t generation)
    {
        requests_.enqueue({ mediaAssetId, filePath, generation });
        startNext();
    }

private:
    struct Request {
        int mediaAssetId = 0;
        QString filePath;
        std::uint64_t generation = 0;
    };

    void ensureDecoder()
    {
        if (decoder_ != nullptr)
            return;

        decoder_ = new QAudioDecoder(this);
        connect(decoder_, &QAudioDecoder::bufferReady,
                this, [this] { consumeBuffer(decoder_->read()); });
        connect(decoder_, &QAudioDecoder::finished,
                this, [this] { finishCurrent(true); });
        connect(decoder_, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error),
                this,
                [this](QAudioDecoder::Error) { finishCurrent(false); });
    }

    void startNext()
    {
        if (isDecoding_ || requests_.isEmpty())
            return;

        ensureDecoder();
        current_ = requests_.dequeue();
        waveform_ = {};
        waveform_.sampleFramesPerPeak = kSampleFramesPerPeak;
        pendingPeak_ = {};
        pendingSampleFrameCount_ = 0;
        isDecoding_ = true;
        decoder_->setSource(QUrl::fromLocalFile(current_.filePath));
        decoder_->start();
    }

    void consumeBuffer(const QAudioBuffer &buffer)
    {
        if (!isDecoding_ || !buffer.isValid())
            return;

        const QAudioFormat format = buffer.format();
        if (!format.isValid())
            return;
        if (waveform_.sampleRate == 0)
            waveform_.sampleRate = format.sampleRate();

        const int channelCount = format.channelCount();
        const int bytesPerSample = format.bytesPerSample();
        const int bytesPerFrame = format.bytesPerFrame();
        const char *data = buffer.constData<char>();
        for (int frame = 0; frame < buffer.frameCount(); ++frame) {
            float frameMinimum = 1.0F;
            float frameMaximum = -1.0F;
            const char *frameData = data + frame * bytesPerFrame;
            for (int channel = 0; channel < channelCount; ++channel) {
                const float sample = format.normalizedSampleValue(
                    frameData + channel * bytesPerSample);
                frameMinimum = std::min(frameMinimum, sample);
                frameMaximum = std::max(frameMaximum, sample);
            }

            if (pendingSampleFrameCount_ == 0)
                pendingPeak_ = { frameMinimum, frameMaximum };
            else {
                pendingPeak_.minimum = std::min(pendingPeak_.minimum,
                                                frameMinimum);
                pendingPeak_.maximum = std::max(pendingPeak_.maximum,
                                                frameMaximum);
            }

            ++pendingSampleFrameCount_;
            if (pendingSampleFrameCount_ == kSampleFramesPerPeak)
                flushPendingPeak();
        }
    }

    void flushPendingPeak()
    {
        if (pendingSampleFrameCount_ == 0)
            return;
        waveform_.peaks.push_back(pendingPeak_);
        pendingPeak_ = {};
        pendingSampleFrameCount_ = 0;
    }

    void finishCurrent(bool succeeded)
    {
        if (!isDecoding_)
            return;

        // QAudioDecoder is a one-request worker here. Reusing the same
        // instance immediately after finished/error is backend-dependent and
        // caused a queued timeline retry to stall on Windows Multimedia.
        // A fresh decoder gives every queued generation an independent,
        // deterministic lifecycle.
        QAudioDecoder *finishedDecoder = decoder_;
        decoder_ = nullptr;
        finishedDecoder->deleteLater();

        flushPendingPeak();
        if (!succeeded)
            waveform_ = {};

        // Report both success and failure. The cache must clear its in-flight
        // marker after a failure so a later timeline placement can retry.
        const int mediaAssetId = current_.mediaAssetId;
        const std::uint64_t generation = current_.generation;
        AudioWaveformData waveform = std::move(waveform_);
        QMetaObject::invokeMethod(
            deliveryContext_,
            [handler = decodedHandler_, mediaAssetId, generation,
             waveform = std::move(waveform)]() mutable {
                handler(mediaAssetId, generation, std::move(waveform));
            },
            Qt::QueuedConnection);
        isDecoding_ = false;
        QTimer::singleShot(0, this, [this] { startNext(); });
    }

    QAudioDecoder *decoder_ = nullptr;
    QObject *deliveryContext_ = nullptr;
    DecodedHandler decodedHandler_;
    QQueue<Request> requests_;
    Request current_;
    AudioWaveformData waveform_;
    AudioWaveformPeak pendingPeak_;
    int pendingSampleFrameCount_ = 0;
    bool isDecoding_ = false;
};

} // namespace

QtAudioWaveformCache::QtAudioWaveformCache(QObject *parent)
    : QObject(parent)
{
    auto *worker = new AudioWaveformDecoderWorker(
        this,
        [this](int mediaAssetId, std::uint64_t generation,
               AudioWaveformData waveform) {
            if (generations_.value(mediaAssetId) != generation)
                return;
            pendingAssetIds_.remove(mediaAssetId);
            if (waveform.sampleRate <= 0 || waveform.peaks.empty())
                return;
            waveforms_.insert(mediaAssetId, std::move(waveform));
            emit waveformChanged(mediaAssetId);
        });
    worker->moveToThread(&workerThread_);
    connect(&workerThread_, &QThread::finished,
            worker, &QObject::deleteLater);
    connect(this, &QtAudioWaveformCache::decodeRequested,
            worker, &AudioWaveformDecoderWorker::enqueue,
            Qt::QueuedConnection);
    workerThread_.start();
}

QtAudioWaveformCache::~QtAudioWaveformCache()
{
    workerThread_.quit();
    workerThread_.wait();
}

void QtAudioWaveformCache::refresh(const MediaLibrary &mediaLibrary)
{
    QHash<int, QString> requestedPaths;
    for (const LibraryMediaAsset &asset : mediaLibrary.assets()) {
        if (asset.kind != MediaKind::Audio || asset.filePath.empty())
            continue;
        requestedPaths.insert(
            asset.id, QString::fromStdWString(asset.filePath.wstring()));
    }

    for (auto iterator = audioFilePaths_.begin();
         iterator != audioFilePaths_.end();) {
        if (requestedPaths.value(iterator.key()) == iterator.value()) {
            ++iterator;
            continue;
        }
        waveforms_.remove(iterator.key());
        pendingAssetIds_.remove(iterator.key());
        generations_.insert(iterator.key(), nextGeneration_++);
        iterator = audioFilePaths_.erase(iterator);
    }

    for (auto iterator = requestedPaths.cbegin();
         iterator != requestedPaths.cend(); ++iterator) {
        const bool pathChanged =
            audioFilePaths_.value(iterator.key()) != iterator.value();
        if (!pathChanged
            && (waveforms_.contains(iterator.key())
                || pendingAssetIds_.contains(iterator.key()))) {
            continue;
        }

        audioFilePaths_.insert(iterator.key(), iterator.value());
        if (pathChanged)
            waveforms_.remove(iterator.key());
        const std::uint64_t generation = nextGeneration_++;
        generations_.insert(iterator.key(), generation);
        pendingAssetIds_.insert(iterator.key());
        emit decodeRequested(iterator.key(), iterator.value(), generation);
    }
}

void QtAudioWaveformCache::requestForTimeline(int mediaAssetId)
{
    if (waveforms_.contains(mediaAssetId))
        return;

    const QString filePath = audioFilePaths_.value(mediaAssetId);
    if (filePath.isEmpty())
        return;

    // A timeline placement is a stronger request than library preloading.
    // Queue a fresh generation even when an earlier decode is still pending:
    // the old completion becomes stale, while this request retries after it.
    const std::uint64_t generation = nextGeneration_++;
    generations_.insert(mediaAssetId, generation);
    pendingAssetIds_.insert(mediaAssetId);
    emit decodeRequested(mediaAssetId, filePath, generation);
}

std::vector<AudioWaveformPeak> QtAudioWaveformCache::peaksForClip(
    const TimelineClip &clip, int pixelWidth,
    int timelineFramesPerSecond) const
{
    const auto waveform = waveforms_.constFind(clip.mediaAssetId);
    if (waveform == waveforms_.constEnd() || waveform->sampleRate <= 0
        || waveform->peaks.empty() || pixelWidth <= 0) {
        return {};
    }

    pixelWidth = std::min(pixelWidth, 8192);
    timelineFramesPerSecond = std::max(1, timelineFramesPerSecond);
    std::vector<AudioWaveformPeak> result(pixelWidth);
    for (int pixel = 0; pixel < pixelWidth; ++pixel) {
        const std::int64_t firstTimelineFrame = clip.state.sourceInFrame
            + static_cast<std::int64_t>(clip.state.durationFrames) * pixel
                / pixelWidth;
        const std::int64_t lastTimelineFrame = clip.state.sourceInFrame
            + static_cast<std::int64_t>(clip.state.durationFrames) * (pixel + 1)
                / pixelWidth;
        const std::int64_t firstSampleFrame = firstTimelineFrame
            * waveform->sampleRate / timelineFramesPerSecond;
        const std::int64_t lastSampleFrame = std::max(
            firstSampleFrame + 1,
            lastTimelineFrame * waveform->sampleRate
                / timelineFramesPerSecond);
        const int firstPeak = std::clamp(
            static_cast<int>(firstSampleFrame / waveform->sampleFramesPerPeak),
            0, static_cast<int>(waveform->peaks.size()) - 1);
        const int lastPeak = std::clamp(
            static_cast<int>((lastSampleFrame - 1)
                             / waveform->sampleFramesPerPeak),
            firstPeak, static_cast<int>(waveform->peaks.size()) - 1);

        AudioWaveformPeak aggregate = waveform->peaks[firstPeak];
        for (int peak = firstPeak + 1; peak <= lastPeak; ++peak) {
            aggregate.minimum = std::min(aggregate.minimum,
                                         waveform->peaks[peak].minimum);
            aggregate.maximum = std::max(aggregate.maximum,
                                         waveform->peaks[peak].maximum);
        }
        result[pixel] = aggregate;
    }
    return result;
}
