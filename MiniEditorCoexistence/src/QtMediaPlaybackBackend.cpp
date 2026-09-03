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

namespace {

bool isPlayableMediaStatus(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferingMedia:
    case QMediaPlayer::BufferedMedia:
        return true;
    default:
        return false;
    }
}

} // namespace

QtMediaPlaybackBackend::QtMediaPlaybackBackend(
    EditorSession &session, MediaLibrary &mediaLibrary)
    : session_(session)
    , mediaLibrary_(mediaLibrary)
    , simulatedBackend_(session)
{
    audioOutput_.setVolume(0.75F);
    player_.setAudioOutput(&audioOutput_);
    timelineAudioOutput_.setVolume(0.75F);
    timelineAudioPlayer_.setAudioOutput(&timelineAudioOutput_);

    QObject::connect(&timelineAudioPlayer_, &QMediaPlayer::mediaStatusChanged,
                     [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::InvalidMedia) {
            stopTimelineAudioPlayback();
            return;
        }
        if (!isPlayableMediaStatus(status)
            || !session_.isTimelineFocused()) {
            return;
        }

        const TimelineAudioPlaybackPlan plan = desiredTimelineAudioPlan();
        if (!plan.hasAudio()
            || plan.mediaAssetId != loadedTimelineAudioAssetId_
            || plan.timelineClipId != loadedTimelineAudioClipId_) {
            return;
        }

        // Loading can finish several timeline ticks after setSource(). Seek
        // to the current plan rather than replaying the now-stale load frame.
        if (hasPendingTimelineAudioSeek_) {
            pendingTimelineAudioSourceFrame_ = plan.sourceFrame;
            timelineAudioPlayer_.setPosition(positionMillisecondsForFrame(
                pendingTimelineAudioSourceFrame_));
            hasPendingTimelineAudioSeek_ = false;
        }
        timelineAudioOutput_.setVolume(
            0.75F * plan.fadeGainPercent / 100.0F);
        timelineAudioOutput_.setMuted(!plan.shouldPlay);
        if (plan.shouldPlay)
            timelineAudioPlayer_.play();
        else
            timelineAudioPlayer_.pause();
    });

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
                hasPendingTimelineSeek_ = false;
                cancelSilentFirstFrameDecode();
                setDecodedVideoVisible(false);
            } else if (isPlayableMediaStatus(status)) {
                // Some backends discard a seek made immediately after
                // setSource(). Reapply the exact trimmed source frame only
                // after the replacement file is ready.
                if (hasPendingTimelineSeek_) {
                    player_.setPosition(positionMillisecondsForFrame(
                        pendingTimelineSeekFrame_));
                    hasPendingTimelineSeek_ = false;
                }

                if (pauseAfterFirstVideoFrame_) {
                    player_.play();
                } else if (session_.timelinePlaybackState().isPlaying) {
                    // Loading and seeking were silent; actual timeline audio
                    // begins only after the requested source frame is applied.
                    audioOutput_.setMuted(shouldMuteVideoTrackAudio());
                    player_.play();
                }
            }
            // EndOfMedia is handled by advanceOneFrame(). It translates the
            // source-media end into a timeline clip boundary, then either
            // switches to the next clip or advances through a gap.
            return;
        }

        if (!isLoadedSourceStillRequested())
            return;

        if (status == QMediaPlayer::EndOfMedia) {
            cancelSilentFirstFrameDecode();
            const int duration = playerDurationFrames();
            session_.updatePlaybackFromBackend(
                duration - 1, duration, false, true);
        } else if (status == QMediaPlayer::InvalidMedia) {
            loadedSourceMediaReady_ = false;
            cancelSilentFirstFrameDecode();
            const PlaybackState &state = session_.playbackState();
            session_.updatePlaybackFromBackend(
                state.currentFrame, state.durationFrames, false, false);
            setDecodedVideoVisible(false);
        } else if (isPlayableMediaStatus(status)) {
            const bool sourceBecameReady = !loadedSourceMediaReady_;
            loadedSourceMediaReady_ = true;
            if (sourceBecameReady) {
                // Some multimedia backends discard a seek issued while the
                // source is still loading. Reapply it once when the source
                // first becomes playable. Later buffering transitions must
                // not jump active playback back to this old position.
                player_.setPosition(positionMillisecondsForFrame(
                    pendingSourceSeekFrame_));
            }

            const MediaPlaybackPlan plan = desiredPlaybackPlan();
            if (pauseAfterFirstVideoFrame_ && sourceBecameReady) {
                // Decode one frame, then pause in the video-sink callback.
                player_.play();
            } else if (plan.shouldPlay
                       && player_.playbackState()
                           != QMediaPlayer::PlayingState) {
                audioOutput_.setMuted(false);
                player_.play();
            }
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
                     [this](const QVideoFrame &frame) {
        const std::optional<PreviewSeekRequest> pendingSeek =
            seekRequests_.current();
        if (!pauseAfterFirstVideoFrame_ && !pendingSeek)
            return;

        // A seek can leave one decoded frame from the old position queued in
        // the sink. Wait until the requested source time arrives before
        // pausing the silent decode.
        const int targetSourceFrame = pendingSeek
            ? pendingSeek->playbackPlan.sourceFrame
            : silentDecodeTargetFrame_;
        const qint64 frameStartMicroseconds = frame.startTime();
        if (frameStartMicroseconds >= 0) {
            const int framesPerSecond = std::max(
                1, session_.playbackState().framesPerSecond);
            const int decodedSourceFrame = static_cast<int>(
                frameStartMicroseconds * framesPerSecond / 1000000);
            constexpr int kSeekToleranceFrames = 2;
            if (std::abs(decodedSourceFrame - targetSourceFrame)
                > kSeekToleranceFrames) {
                return;
            }
        }

        if (pendingSeek) {
            const PreviewSeekResult result{
                pendingSeek->requestId, targetSourceFrame
            };
            if (!seekRequests_.accepts(result))
                return;
            seekRequests_.complete(result.requestId);
        }

        if (pauseAfterFirstVideoFrame_)
            finishSilentFirstFrameDecode();
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
    return PlaybackClockController::tickIntervalMillisecondsForRate(
        session_.playbackState().playbackRatePercent);
}

PlaybackClockAction QtMediaPlaybackBackend::executeCommand(
    LegacyPlaybackCommand command)
{
    if (session_.isTimelineFocused()) {
        const PlaybackState stateBeforeCommand = session_.timelinePlaybackState();
        session_.handlePlaybackCommand(command);
        const bool isStartingPlayback =
            command == LegacyPlaybackCommand::TogglePlayPause
            && !stateBeforeCommand.isPlaying;

        if (command == LegacyPlaybackCommand::Stop) {
            stopRealPlayback();
            stopTimelineAudioPlayback();
        } else if (stateBeforeCommand.isPlaying
                   && command == LegacyPlaybackCommand::TogglePlayPause) {
            player_.pause();
            timelineAudioPlayer_.pause();
        }

        // A stopped edit preview may intentionally show the focused clip's
        // first frame while the head remains elsewhere. Starting playback
        // must therefore seek once to the head before player_.play().
        const PlaybackClockAction action =
            synchronizeTimelinePlayback(isStartingPlayback);
        synchronizeTimelineAudio(desiredTimelineAudioPlan(),
                                 isStartingPlayback);
        return action;
    }

    stopTimelineAudioPlayback();

    const MediaPlaybackPlan plan = desiredPlaybackPlan();
    if (plan.context != MediaPlaybackContext::Source
        || !plan.usesMediaDecoder()) {
        stopRealPlayback();
        return simulatedBackend_.executeCommand(command);
    }

    if (!ensureSelectedSourceLoaded(plan))
        return simulatedBackend_.executeCommand(command);

    const PlaybackState stateBeforeCommand = session_.playbackState();
    session_.handlePlaybackCommand(command);

    switch (command) {
    case LegacyPlaybackCommand::TogglePlayPause:
        if (stateBeforeCommand.isPlaying) {
            player_.pause();
        } else {
            cancelSilentFirstFrameDecode();
            player_.setPosition(positionMillisecondsForFrame(
                session_.playbackState().currentFrame));
            player_.play();
        }
        break;
    case LegacyPlaybackCommand::Stop:
        cancelSilentFirstFrameDecode();
        player_.stop();
        player_.setPosition(0);
        break;
    case LegacyPlaybackCommand::StepBackward:
    case LegacyPlaybackCommand::StepForward:
        player_.pause();
        player_.setPosition(positionMillisecondsForFrame(
            session_.playbackState().currentFrame));
        break;
    }

    return synchronize();
}

PlaybackClockAction QtMediaPlaybackBackend::synchronize()
{
    applyPlaybackRate();
    if (session_.isTimelineFocused()) {
        const PlaybackClockAction action = synchronizeTimelinePlayback();
        synchronizeTimelineAudio(desiredTimelineAudioPlan(), false);
        return action;
    }

    stopTimelineAudioPlayback();

    const MediaPlaybackPlan plan = desiredPlaybackPlan();
    if (plan.context != MediaPlaybackContext::Source
        || !plan.usesMediaDecoder()) {
        stopRealPlayback();
        return simulatedBackend_.synchronize();
    }

    ensureSelectedSourceLoaded(plan);
    const bool metadataIsLoading =
        player_.mediaStatus() == QMediaPlayer::LoadingMedia;
    return session_.playbackState().isPlaying || metadataIsLoading
        || pauseAfterFirstVideoFrame_
        ? PlaybackClockAction::EnsureRunning
        : PlaybackClockAction::Stop;
}

PlaybackClockAction QtMediaPlaybackBackend::seek(
    const PreviewSeekRequest &request)
{
    if (!seekRequests_.begin(request))
        return synchronize();

    const MediaPlaybackPlan &plan = request.playbackPlan;
    if (request.context == MediaPlaybackContext::Timeline) {
        const PlaybackClockAction action =
            synchronizeTimelinePlayback(plan, true);
        synchronizeTimelineAudio(request.audioPlaybackPlan, true);
        return action;
    }

    stopTimelineAudioPlayback();

    if (plan.context != MediaPlaybackContext::Source
        || !plan.usesMediaDecoder()) {
        seekRequests_.complete(request.requestId);
        return simulatedBackend_.seek(request);
    }

    pendingSourceSeekFrame_ = plan.sourceFrame;
    if (!ensureSelectedSourceLoaded(plan)) {
        seekRequests_.complete(request.requestId);
        return simulatedBackend_.seek(request);
    }

    player_.setPosition(positionMillisecondsForFrame(pendingSourceSeekFrame_));
    if (plan.mediaKind != MediaKind::Video)
        seekRequests_.complete(request.requestId);
    if (plan.needsSilentVideoPreroll()) {
        // Decode the newly requested paused frame. The video-sink callback
        // pauses the player as soon as that frame becomes available.
        beginSilentFrameDecode(pendingSourceSeekFrame_);
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

        if (!session_.timelinePlaybackState().isPlaying) {
            const PlaybackClockAction action = synchronizeTimelinePlayback();
            synchronizeTimelineAudio(desiredTimelineAudioPlan(), false);
            return action;
        }

        // This advances video clips, still images, and gaps consistently. The
        // subsequent synchronisation maps the new head position to a decoded
        // video clip when one exists, and switches the player only at a clip
        // boundary.
        simulatedBackend_.advanceOneFrame();
        const PlaybackClockAction action = synchronizeTimelinePlayback();
        synchronizeTimelineAudio(desiredTimelineAudioPlan(), false);
        return action;
    }

    stopTimelineAudioPlayback();

    const MediaPlaybackPlan plan = desiredPlaybackPlan();
    if (plan.context != MediaPlaybackContext::Source
        || !plan.usesMediaDecoder()) {
        return simulatedBackend_.advanceOneFrame();
    }

    // MFC owns the outer message loop. Its playback timer periodically gives
    // queued Qt Multimedia callbacks a chance to reach the UI thread; media
    // position itself still comes from QMediaPlayer, never from this tick.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
    return synchronize();
}

MediaPlaybackPlan QtMediaPlaybackBackend::desiredPlaybackPlan() const
{
    return MediaPlaybackPlanResolver::resolve(session_, mediaLibrary_);
}

bool QtMediaPlaybackBackend::ensureSelectedSourceLoaded(
    const MediaPlaybackPlan &plan)
{
    if (plan.context != MediaPlaybackContext::Source
        || !plan.usesMediaDecoder()) {
        return false;
    }

    const LibraryMediaAsset *asset = mediaLibrary_.findAsset(plan.mediaAssetId);
    if (asset == nullptr || asset->filePath.empty())
        return false;

    std::error_code error;
    if (!std::filesystem::is_regular_file(asset->filePath, error))
        return false;

    if (loadedAssetId_ != asset->id || loadedForTimeline_) {
        // Drop the previous timeline frame while the newly selected source
        // loads. The preview will show the new frame only after it arrives.
        // Mark the replacement as not ready before stopping the old source.
        // QMediaPlayer may synchronously or asynchronously publish its final
        // old position; updateSessionFromPlayer must ignore that callback.
        loadedSourceMediaReady_ = false;
        setDecodedVideoVisible(false);
        player_.stop();
        loadedAssetId_ = asset->id;
        loadedTimelineClipId_ = 0;
        loadedForTimeline_ = false;
        hasPendingTimelineSeek_ = false;
        if (plan.needsSilentVideoPreroll())
            beginSilentFrameDecode(plan.sourceFrame);
        else
            cancelSilentFirstFrameDecode();
        pendingSourceSeekFrame_ = plan.sourceFrame;
        player_.setSource(QUrl::fromLocalFile(
            QString::fromStdWString(asset->filePath.wstring())));
        // A paused QMediaPlayer does not necessarily decode until it receives
        // an explicit seek. This requests the first frame as soon as loading
        // completes, so selecting a video shows a useful preview before Play.
        player_.setPosition(positionMillisecondsForFrame(plan.sourceFrame));
    }

    setDecodedVideoVisible(asset->kind == MediaKind::Video);
    return true;
}

bool QtMediaPlaybackBackend::isLoadedSourceStillSelected() const
{
    return loadedSourceMediaReady_ && isLoadedSourceStillRequested();
}

bool QtMediaPlaybackBackend::isLoadedSourceStillRequested() const
{
    const MediaPlaybackPlan plan = desiredPlaybackPlan();
    return !loadedForTimeline_
        && plan.context == MediaPlaybackContext::Source
        && plan.usesMediaDecoder()
        && plan.mediaAssetId == loadedAssetId_;
}

bool QtMediaPlaybackBackend::ensureTimelineVideoLoaded(
    const MediaPlaybackPlan &plan, bool seekToTimelineFrame)
{
    if (plan.context != MediaPlaybackContext::Timeline
        || plan.mediaKind != MediaKind::Video) {
        return false;
    }

    const LibraryMediaAsset *asset = mediaLibrary_.findAsset(plan.mediaAssetId);
    if (asset == nullptr || asset->filePath.empty())
        return false;

    std::error_code error;
    if (!std::filesystem::is_regular_file(asset->filePath, error))
        return false;

    const bool changedClip = !loadedForTimeline_
        || loadedAssetId_ != asset->id
        || loadedTimelineClipId_ != plan.timelineClipId;
    if (changedClip) {
        setDecodedVideoVisible(false);
        // A stopped timeline selection still needs one decoded frame. Playing
        // briefly is the reliable cross-backend way to make QVideoSink receive
        // it; the sink callback pauses immediately after that frame arrives.
        if (!plan.needsSilentVideoPreroll())
            cancelSilentFirstFrameDecode();
        player_.stop();
        loadedAssetId_ = asset->id;
        loadedTimelineClipId_ = plan.timelineClipId;
        loadedForTimeline_ = true;
        pendingTimelineSeekFrame_ = plan.sourceFrame;
        hasPendingTimelineSeek_ = true;
        // Do not expose frame-zero audio while the decoder is loading and has
        // not yet accepted the trimmed source seek.
        audioOutput_.setMuted(true);
        player_.setSource(QUrl::fromLocalFile(
            QString::fromStdWString(asset->filePath.wstring())));
    }

    if ((changedClip || seekToTimelineFrame)
        && plan.needsSilentVideoPreroll()) {
        beginSilentFrameDecode(plan.sourceFrame);
    }

    setDecodedVideoVisible(true);
    if (changedClip || seekToTimelineFrame) {
        player_.setPosition(positionMillisecondsForFrame(plan.sourceFrame));
    }
    return true;
}

bool QtMediaPlaybackBackend::isLoadedTimelineVideoStillActive() const
{
    const MediaPlaybackPlan plan = desiredPlaybackPlan();
    return loadedForTimeline_
        && plan.context == MediaPlaybackContext::Timeline
        && plan.mediaKind == MediaKind::Video
        && plan.mediaAssetId == loadedAssetId_
        && plan.timelineClipId == loadedTimelineClipId_;
}

PlaybackClockAction QtMediaPlaybackBackend::synchronizeTimelinePlayback(
    bool seekToTimelineFrame)
{
    const MediaPlaybackPlan plan = desiredPlaybackPlan();
    return synchronizeTimelinePlayback(plan, seekToTimelineFrame);
}

PlaybackClockAction QtMediaPlaybackBackend::synchronizeTimelinePlayback(
    const MediaPlaybackPlan &plan, bool seekToTimelineFrame)
{
    const PlaybackState &timeline = session_.timelinePlaybackState();
    if (!ensureTimelineVideoLoaded(
            plan, seekToTimelineFrame || !timeline.isPlaying)) {
        if (seekToTimelineFrame && seekRequests_.current())
            seekRequests_.complete(seekRequests_.current()->requestId);
        stopRealPlayback();
        return timeline.isPlaying ? PlaybackClockAction::EnsureRunning
                                  : PlaybackClockAction::Stop;
    }

    if (timeline.isPlaying) {
        cancelSilentFirstFrameDecode();
        audioOutput_.setMuted(shouldMuteVideoTrackAudio());
        player_.play();
        return PlaybackClockAction::EnsureRunning;
    }

    if (pauseAfterFirstVideoFrame_) {
        player_.play();
        return PlaybackClockAction::EnsureRunning;
    }

    player_.pause();
    return PlaybackClockAction::Stop;
}

TimelineAudioPlaybackPlan QtMediaPlaybackBackend::desiredTimelineAudioPlan() const
{
    return TimelineAudioPlaybackPlanResolver::resolve(session_, mediaLibrary_);
}

bool QtMediaPlaybackBackend::ensureTimelineAudioLoaded(
    const TimelineAudioPlaybackPlan &plan, bool seekToTimelineFrame)
{
    if (!plan.hasAudio())
        return false;

    const LibraryMediaAsset *asset = mediaLibrary_.findAsset(plan.mediaAssetId);
    if (asset == nullptr || asset->kind != MediaKind::Audio
        || asset->filePath.empty()) {
        return false;
    }

    std::error_code error;
    if (!std::filesystem::is_regular_file(asset->filePath, error))
        return false;

    const bool changedClip = loadedTimelineAudioAssetId_ != asset->id
        || loadedTimelineAudioClipId_ != plan.timelineClipId;
    if (changedClip) {
        timelineAudioOutput_.setMuted(true);
        timelineAudioPlayer_.stop();
        loadedTimelineAudioAssetId_ = asset->id;
        loadedTimelineAudioClipId_ = plan.timelineClipId;
        pendingTimelineAudioSourceFrame_ = plan.sourceFrame;
        hasPendingTimelineAudioSeek_ = true;
        timelineAudioPlayer_.setSource(QUrl::fromLocalFile(
            QString::fromStdWString(asset->filePath.wstring())));
    }

    if (changedClip || seekToTimelineFrame) {
        pendingTimelineAudioSourceFrame_ = plan.sourceFrame;
        hasPendingTimelineAudioSeek_ = true;
        timelineAudioPlayer_.setPosition(positionMillisecondsForFrame(
            pendingTimelineAudioSourceFrame_));
    }
    return true;
}

void QtMediaPlaybackBackend::synchronizeTimelineAudio(
    const TimelineAudioPlaybackPlan &plan, bool seekToTimelineFrame)
{
    if (!ensureTimelineAudioLoaded(plan, seekToTimelineFrame)) {
        stopTimelineAudioPlayback();
        return;
    }

    timelineAudioOutput_.setVolume(
        0.75F * plan.fadeGainPercent / 100.0F);
    if (plan.shouldPlay) {
        timelineAudioOutput_.setMuted(false);
        timelineAudioPlayer_.play();
    } else {
        // Seeking while stopped or paused prepares the decoder position but
        // must never leak a short packet through the speakers.
        timelineAudioOutput_.setMuted(true);
        timelineAudioPlayer_.pause();
    }
}

void QtMediaPlaybackBackend::stopTimelineAudioPlayback()
{
    timelineAudioOutput_.setMuted(true);
    if (timelineAudioPlayer_.playbackState() != QMediaPlayer::StoppedState)
        timelineAudioPlayer_.stop();
    timelineAudioPlayer_.setSource({});
    loadedTimelineAudioAssetId_ = 0;
    loadedTimelineAudioClipId_ = 0;
    pendingTimelineAudioSourceFrame_ = 0;
    hasPendingTimelineAudioSeek_ = false;
}

void QtMediaPlaybackBackend::beginSilentFrameDecode(int targetSourceFrame)
{
    // QMediaPlayer must briefly play to make some backends deliver a paused
    // frame to QVideoSink. That hidden preroll is visual work only: muting it
    // prevents a tiny repeated audio packet from reaching the speakers.
    pauseAfterFirstVideoFrame_ = true;
    silentDecodeTargetFrame_ = std::max(0, targetSourceFrame);
    audioOutput_.setMuted(true);
}

void QtMediaPlaybackBackend::finishSilentFirstFrameDecode()
{
    if (!pauseAfterFirstVideoFrame_)
        return;

    // Pause before restoring audio so no queued preroll sample becomes audible.
    player_.pause();
    cancelSilentFirstFrameDecode();
}

void QtMediaPlaybackBackend::cancelSilentFirstFrameDecode()
{
    pauseAfterFirstVideoFrame_ = false;
    if (!hasPendingTimelineSeek_)
        audioOutput_.setMuted(shouldMuteVideoTrackAudio());
}

bool QtMediaPlaybackBackend::shouldMuteVideoTrackAudio() const
{
    return session_.isTimelineFocused()
        && session_.timelineAudioMixState().isVideoTrackMuted;
}

void QtMediaPlaybackBackend::applyPlaybackRate()
{
    const qreal rate = session_.playbackState().playbackRatePercent / 100.0;
    if (!qFuzzyCompare(player_.playbackRate(), rate))
        player_.setPlaybackRate(rate);
    if (!qFuzzyCompare(timelineAudioPlayer_.playbackRate(), rate))
        timelineAudioPlayer_.setPlaybackRate(rate);
}

void QtMediaPlaybackBackend::stopRealPlayback()
{
    if (player_.playbackState() != QMediaPlayer::StoppedState)
        player_.stop();
    loadedTimelineClipId_ = 0;
    loadedForTimeline_ = false;
    loadedSourceMediaReady_ = false;
    hasPendingTimelineSeek_ = false;
    seekRequests_.clear();
    cancelSilentFirstFrameDecode();
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
    // Fallback for media backends that report the first position before they
    // deliver a QVideoSink frame. Keep this ahead of the stopped/paused guard:
    // silent preroll intentionally runs while the editor is stopped.
    if (pauseAfterFirstVideoFrame_ && player_.position() > 0)
        finishSilentFirstFrameDecode();

    if (!state.isPlaying) {
        // Seeking, changing a selection, and the silent first-frame decode
        // all put the desired source frame into EditorSession before asking
        // QMediaPlayer to decode it. A queued positionChanged signal has no
        // source ID, so it can belong to the previous file. Do not let it
        // overwrite an intentional stopped/paused preview position.
        session_.updatePlaybackFromBackend(
            state.currentFrame, durationFrames, false, state.isPaused);
        return;
    }

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
