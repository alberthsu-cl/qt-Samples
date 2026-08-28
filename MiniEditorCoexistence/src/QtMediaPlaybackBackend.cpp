#include "QtMediaPlaybackBackend.h"

#include "EditorSession.h"
#include "MediaLibrary.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QUrl>
#include <QVideoSink>

#include <algorithm>
#include <filesystem>
#include <utility>

QtMediaPlaybackBackend::QtMediaPlaybackBackend(
    EditorSession &session, MediaLibrary &mediaLibrary)
    : session_(session)
    , mediaLibrary_(mediaLibrary)
    , simulatedBackend_(session)
{
    audioOutput_.setVolume(0.75F);
    player_.setAudioOutput(&audioOutput_);

    QObject::connect(&player_, &QMediaPlayer::positionChanged,
                     [this](qint64) { updateSessionFromPlayer(); });
    QObject::connect(&player_, &QMediaPlayer::durationChanged,
                     [this](qint64) { updateSessionFromPlayer(); });
    QObject::connect(&player_, &QMediaPlayer::mediaStatusChanged,
                     [this](QMediaPlayer::MediaStatus status) {
        if (!isLoadedSourceStillSelected())
            return;

        if (status == QMediaPlayer::EndOfMedia) {
            const int duration = playerDurationFrames();
            session_.updatePlaybackFromBackend(
                duration - 1, duration, false, true);
        } else if (status == QMediaPlayer::InvalidMedia) {
            const PlaybackState &state = session_.playbackState();
            session_.updatePlaybackFromBackend(
                state.currentFrame, state.durationFrames, false, false);
            setDecodedVideoVisible(false);
        }
    });
}

void QtMediaPlaybackBackend::setVideoOutput(QVideoSink *videoSink)
{
    player_.setVideoOutput(videoSink);
}

void QtMediaPlaybackBackend::setVideoVisibilityHandler(
    VideoVisibilityHandler handler)
{
    videoVisibilityHandler_ = std::move(handler);
    if (videoVisibilityHandler_)
        videoVisibilityHandler_(decodedVideoVisible_);
}

void QtMediaPlaybackBackend::setSourceMetadataChangedHandler(
    SourceMetadataChangedHandler handler)
{
    sourceMetadataChangedHandler_ = std::move(handler);
}

unsigned int QtMediaPlaybackBackend::tickIntervalMilliseconds() const
{
    return PlaybackClockController::kTickIntervalMilliseconds;
}

PlaybackClockAction QtMediaPlaybackBackend::executeCommand(
    PlaybackCommand command)
{
    if (selectedRealSource() == nullptr) {
        stopRealPlayback();
        return simulatedBackend_.executeCommand(command);
    }

    if (!ensureSelectedSourceLoaded())
        return simulatedBackend_.executeCommand(command);

    const PlaybackState stateBeforeCommand = session_.playbackState();
    session_.handlePlaybackCommand(command);

    switch (command) {
    case PlaybackCommand::TogglePlayPause:
        if (stateBeforeCommand.isPlaying) {
            player_.pause();
        } else {
            player_.setPosition(positionMillisecondsForFrame(
                session_.playbackState().currentFrame));
            player_.play();
        }
        break;
    case PlaybackCommand::Stop:
        player_.stop();
        player_.setPosition(0);
        break;
    case PlaybackCommand::StepBackward:
    case PlaybackCommand::StepForward:
        player_.pause();
        player_.setPosition(positionMillisecondsForFrame(
            session_.playbackState().currentFrame));
        break;
    }

    return synchronize();
}

PlaybackClockAction QtMediaPlaybackBackend::synchronize()
{
    if (selectedRealSource() == nullptr) {
        stopRealPlayback();
        return simulatedBackend_.synchronize();
    }

    ensureSelectedSourceLoaded();
    const bool metadataIsLoading =
        player_.mediaStatus() == QMediaPlayer::LoadingMedia;
    return session_.playbackState().isPlaying || metadataIsLoading
        ? PlaybackClockAction::EnsureRunning
        : PlaybackClockAction::Stop;
}

PlaybackClockAction QtMediaPlaybackBackend::advanceOneFrame()
{
    if (selectedRealSource() == nullptr)
        return simulatedBackend_.advanceOneFrame();

    // MFC owns the outer message loop. Its playback timer periodically gives
    // queued Qt Multimedia callbacks a chance to reach the UI thread; media
    // position itself still comes from QMediaPlayer, never from this tick.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
    return synchronize();
}

const LibraryMediaAsset *QtMediaPlaybackBackend::selectedRealSource() const
{
    if (session_.isTimelineFocused())
        return nullptr;

    const auto &assets = mediaLibrary_.assets();
    const int index = session_.selectedAssetIndex();
    if (index < 0 || index >= static_cast<int>(assets.size()))
        return nullptr;

    const LibraryMediaAsset &asset = assets[index];
    if (asset.kind == MediaKind::Image || asset.filePath.empty())
        return nullptr;

    std::error_code error;
    return std::filesystem::is_regular_file(asset.filePath, error)
        ? &asset : nullptr;
}

bool QtMediaPlaybackBackend::ensureSelectedSourceLoaded()
{
    const LibraryMediaAsset *asset = selectedRealSource();
    if (asset == nullptr)
        return false;

    if (loadedAssetId_ != asset->id) {
        player_.stop();
        loadedAssetId_ = asset->id;
        player_.setSource(QUrl::fromLocalFile(
            QString::fromStdWString(asset->filePath.wstring())));
    }

    setDecodedVideoVisible(asset->kind == MediaKind::Video);
    return true;
}

bool QtMediaPlaybackBackend::isLoadedSourceStillSelected() const
{
    const LibraryMediaAsset *asset = selectedRealSource();
    return asset != nullptr && asset->id == loadedAssetId_;
}

void QtMediaPlaybackBackend::stopRealPlayback()
{
    if (player_.playbackState() != QMediaPlayer::StoppedState)
        player_.stop();
    setDecodedVideoVisible(false);
}

void QtMediaPlaybackBackend::updateSessionFromPlayer()
{
    if (!isLoadedSourceStillSelected())
        return;

    const int durationFrames = playerDurationFrames();
    if (player_.duration() > 0
        && mediaLibrary_.updateAssetDuration(loadedAssetId_, durationFrames)
        && sourceMetadataChangedHandler_) {
        sourceMetadataChangedHandler_();
    }

    const PlaybackState &state = session_.playbackState();
    session_.updatePlaybackFromBackend(
        playerPositionFrame(), durationFrames,
        state.isPlaying, state.isPaused);
}

void QtMediaPlaybackBackend::setDecodedVideoVisible(bool visible)
{
    if (decodedVideoVisible_ == visible)
        return;
    decodedVideoVisible_ = visible;
    if (videoVisibilityHandler_)
        videoVisibilityHandler_(visible);
}

int QtMediaPlaybackBackend::playerDurationFrames() const
{
    const int framesPerSecond = std::max(
        1, session_.playbackState().framesPerSecond);
    if (player_.duration() <= 0)
        return std::max(1, session_.playbackState().durationFrames);
    return std::max(1, static_cast<int>(
        (player_.duration() * framesPerSecond + 500) / 1000));
}

int QtMediaPlaybackBackend::playerPositionFrame() const
{
    const int framesPerSecond = std::max(
        1, session_.playbackState().framesPerSecond);
    return std::clamp(static_cast<int>(
        player_.position() * framesPerSecond / 1000),
        0, playerDurationFrames() - 1);
}

qint64 QtMediaPlaybackBackend::positionMillisecondsForFrame(int frame) const
{
    const int framesPerSecond = std::max(
        1, session_.playbackState().framesPerSecond);
    return static_cast<qint64>(std::max(0, frame)) * 1000
        / framesPerSecond;
}
