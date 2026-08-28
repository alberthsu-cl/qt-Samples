#include "QtMediaPlaybackBackend.h"

#include "EditorSession.h"
#include "MediaLibrary.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QUrl>
#include <QVideoFrame>
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
        if (session_.isTimelineFocused()) {
            if (!isLoadedTimelineVideoStillActive())
                return;

            if (status == QMediaPlayer::InvalidMedia) {
                const PlaybackState &timeline = session_.timelinePlaybackState();
                session_.updatePlaybackFromBackend(
                    timeline.currentFrame, timeline.durationFrames, false, false);
                setDecodedVideoVisible(false);
            }
            // EndOfMedia is handled by advanceOneFrame(). It translates the
            // source-media end into a timeline clip boundary, then either
            // switches to the next clip or advances through a gap.
            return;
        }

        if (!isLoadedSourceStillSelected())
            return;

        if (status == QMediaPlayer::EndOfMedia) {
            pauseAfterFirstSourceVideoFrame_ = false;
            const int duration = playerDurationFrames();
            session_.updatePlaybackFromBackend(
                duration - 1, duration, false, true);
        } else if (status == QMediaPlayer::InvalidMedia) {
            pauseAfterFirstSourceVideoFrame_ = false;
            const PlaybackState &state = session_.playbackState();
            session_.updatePlaybackFromBackend(
                state.currentFrame, state.durationFrames, false, false);
            setDecodedVideoVisible(false);
        } else if (status == QMediaPlayer::LoadedMedia
                   && pauseAfterFirstSourceVideoFrame_) {
            // Some multimedia backends discard a seek issued while the source
            // is still loading. Seek again after LoadedMedia, then decode one
            // frame and pause it in the video-sink callback.
            player_.setPosition(positionMillisecondsForFrame(
                pendingSourceSeekFrame_));
            player_.play();
        }
    });
}

void QtMediaPlaybackBackend::setVideoOutput(QVideoSink *videoSink)
{
    player_.setVideoOutput(videoSink);
    if (videoSink == nullptr)
        return;

    // A player paused at position zero may never decode a frame. For source
    // selection, begin decoding briefly and pause as soon as frame zero reaches
    // the preview sink, so the user sees a still preview before pressing Play.
    QObject::connect(videoSink, &QVideoSink::videoFrameChanged,
                     [this](const QVideoFrame &) {
        if (!pauseAfterFirstSourceVideoFrame_ || session_.isTimelineFocused())
            return;

        pauseAfterFirstSourceVideoFrame_ = false;
        player_.pause();
    });
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
    if (session_.isTimelineFocused()) {
        const PlaybackState stateBeforeCommand = session_.timelinePlaybackState();
        session_.handlePlaybackCommand(command);

        if (command == PlaybackCommand::Stop) {
            stopRealPlayback();
        } else if (stateBeforeCommand.isPlaying
                   && command == PlaybackCommand::TogglePlayPause) {
            player_.pause();
        }

        return synchronizeTimelinePlayback();
    }

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
    if (session_.isTimelineFocused())
        return synchronizeTimelinePlayback();

    if (selectedRealSource() == nullptr) {
        stopRealPlayback();
        return simulatedBackend_.synchronize();
    }

    ensureSelectedSourceLoaded();
    const bool metadataIsLoading =
        player_.mediaStatus() == QMediaPlayer::LoadingMedia;
    return session_.playbackState().isPlaying || metadataIsLoading
        || pauseAfterFirstSourceVideoFrame_
        ? PlaybackClockAction::EnsureRunning
        : PlaybackClockAction::Stop;
}

PlaybackClockAction QtMediaPlaybackBackend::seekToCurrentFrame()
{
    if (session_.isTimelineFocused()) {
        ensureTimelineVideoLoaded(true);
        return synchronizeTimelinePlayback();
    }

    const LibraryMediaAsset *asset = selectedRealSource();
    if (asset == nullptr)
        return simulatedBackend_.seekToCurrentFrame();

    pendingSourceSeekFrame_ = session_.playbackState().currentFrame;
    if (!ensureSelectedSourceLoaded())
        return simulatedBackend_.seekToCurrentFrame();

    player_.setPosition(positionMillisecondsForFrame(pendingSourceSeekFrame_));
    if (asset->kind == MediaKind::Video && !session_.playbackState().isPlaying) {
        // Decode the newly requested paused frame. The video-sink callback
        // pauses the player as soon as that frame becomes available.
        pauseAfterFirstSourceVideoFrame_ = true;
        player_.play();
    }
    return synchronize();
}

PlaybackClockAction QtMediaPlaybackBackend::advanceOneFrame()
{
    if (session_.isTimelineFocused()) {
        // MFC owns the outer message loop. Giving Qt a short event-processing
        // opportunity keeps the video decoder responsive. The timeline clock
        // itself is deliberately not taken from QMediaPlayer: it is the one
        // authoritative source for the head and the transport progress bar.
        QCoreApplication::processEvents(QEventLoop::AllEvents, 2);

        if (!session_.timelinePlaybackState().isPlaying)
            return synchronizeTimelinePlayback();

        // This advances video clips, still images, and gaps consistently. The
        // subsequent synchronisation maps the new head position to a decoded
        // video clip when one exists, and switches the player only at a clip
        // boundary.
        simulatedBackend_.advanceOneFrame();
        return synchronizeTimelinePlayback();
    }

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

    if (loadedAssetId_ != asset->id || loadedForTimeline_) {
        // Drop the previous timeline frame while the newly selected source
        // loads. The preview will show the new frame only after it arrives.
        setDecodedVideoVisible(false);
        player_.stop();
        loadedAssetId_ = asset->id;
        loadedTimelineClipId_ = 0;
        loadedForTimeline_ = false;
        pauseAfterFirstSourceVideoFrame_ = asset->kind == MediaKind::Video;
        pendingSourceSeekFrame_ = session_.playbackState().currentFrame;
        player_.setSource(QUrl::fromLocalFile(
            QString::fromStdWString(asset->filePath.wstring())));
        // A paused QMediaPlayer does not necessarily decode until it receives
        // an explicit seek. This requests the first frame as soon as loading
        // completes, so selecting a video shows a useful preview before Play.
        player_.setPosition(0);
    }

    setDecodedVideoVisible(asset->kind == MediaKind::Video);
    return true;
}

bool QtMediaPlaybackBackend::isLoadedSourceStillSelected() const
{
    const LibraryMediaAsset *asset = selectedRealSource();
    return !loadedForTimeline_ && asset != nullptr && asset->id == loadedAssetId_;
}

std::optional<ResolvedTimelineMedia> QtMediaPlaybackBackend::selectedTimelineVideo() const
{
    if (!session_.isTimelineFocused())
        return std::nullopt;

    return TimelinePlaybackResolver::resolve(
        session_.timelineModel(), mediaLibrary_,
        session_.timelinePlaybackState().currentFrame).video;
}

bool QtMediaPlaybackBackend::ensureTimelineVideoLoaded(bool seekToTimelineFrame)
{
    const std::optional<ResolvedTimelineMedia> video = selectedTimelineVideo();
    if (!video || video->mediaKind != MediaKind::Video)
        return false;

    const LibraryMediaAsset *asset = mediaLibrary_.findAsset(video->mediaAssetId);
    if (asset == nullptr || asset->filePath.empty())
        return false;

    std::error_code error;
    if (!std::filesystem::is_regular_file(asset->filePath, error))
        return false;

    const bool changedClip = !loadedForTimeline_
        || loadedAssetId_ != asset->id || loadedTimelineClipId_ != video->clipId;
    if (changedClip) {
        setDecodedVideoVisible(false);
        pauseAfterFirstSourceVideoFrame_ = false;
        player_.stop();
        loadedAssetId_ = asset->id;
        loadedTimelineClipId_ = video->clipId;
        loadedForTimeline_ = true;
        player_.setSource(QUrl::fromLocalFile(
            QString::fromStdWString(asset->filePath.wstring())));
    }

    setDecodedVideoVisible(true);
    if (changedClip || seekToTimelineFrame) {
        player_.setPosition(positionMillisecondsForFrame(video->sourceFrame));
    }
    return true;
}

bool QtMediaPlaybackBackend::isLoadedTimelineVideoStillActive() const
{
    const std::optional<ResolvedTimelineMedia> video = selectedTimelineVideo();
    return loadedForTimeline_ && video && video->mediaKind == MediaKind::Video
        && video->mediaAssetId == loadedAssetId_
        && video->clipId == loadedTimelineClipId_;
}

PlaybackClockAction QtMediaPlaybackBackend::synchronizeTimelinePlayback()
{
    const PlaybackState &timeline = session_.timelinePlaybackState();
    if (!ensureTimelineVideoLoaded(!timeline.isPlaying)) {
        stopRealPlayback();
        return timeline.isPlaying ? PlaybackClockAction::EnsureRunning
                                  : PlaybackClockAction::Stop;
    }

    if (timeline.isPlaying) {
        player_.play();
        return PlaybackClockAction::EnsureRunning;
    }

    player_.pause();
    return PlaybackClockAction::Stop;
}

void QtMediaPlaybackBackend::stopRealPlayback()
{
    if (player_.playbackState() != QMediaPlayer::StoppedState)
        player_.stop();
    loadedTimelineClipId_ = 0;
    loadedForTimeline_ = false;
    pauseAfterFirstSourceVideoFrame_ = false;
    setDecodedVideoVisible(false);
}

void QtMediaPlaybackBackend::updateSessionFromPlayer()
{
    if (session_.isTimelineFocused()) {
        // Timeline head/progress is owned by SimulatedPlaybackBackend. The
        // media player is only the decoded picture source for that head.
        return;
    }

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

    // Fallback for media backends that report the first position before they
    // deliver a QVideoSink frame.
    if (pauseAfterFirstSourceVideoFrame_ && player_.position() > 0) {
        pauseAfterFirstSourceVideoFrame_ = false;
        player_.pause();
    }
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
    return std::clamp(unclampedPlayerPositionFrame(),
                      0, playerDurationFrames() - 1);
}

int QtMediaPlaybackBackend::unclampedPlayerPositionFrame() const
{
    const int framesPerSecond = std::max(
        1, session_.playbackState().framesPerSecond);
    return std::max(0, static_cast<int>(
        player_.position() * framesPerSecond / 1000));
}

qint64 QtMediaPlaybackBackend::positionMillisecondsForFrame(int frame) const
{
    const int framesPerSecond = std::max(
        1, session_.playbackState().framesPerSecond);
    return static_cast<qint64>(std::max(0, frame)) * 1000
        / framesPerSecond;
}
