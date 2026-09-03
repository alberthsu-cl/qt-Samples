#pragma once

#include "MediaPlaybackPlan.h"
#include "PlaybackBackend.h"
#include "TimelineAudioPlaybackPlan.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>

#include <atomic>
#include <functional>

class EditorSession;
class MediaLibrary;
class QVideoFrame;
class QVideoSink;

// Qt Multimedia playback for a selected source video/audio and the active
// video clip on the timeline. Editor decisions remain in the framework-
// neutral MediaPlaybackPlanResolver; this class only validates, loads, seeks,
// and plays the media described by that plan.
class QtMediaPlaybackBackend final : public IPlaybackBackend
{
public:
    using VideoVisibilityHandler = std::function<void(bool visible)>;
    using SourceMetadataChangedHandler = std::function<void()>;

    QtMediaPlaybackBackend(EditorSession &session,
                           MediaLibrary &mediaLibrary);

    void setVideoOutput(QVideoSink *videoSink);
    void setVideoVisibilityHandler(VideoVisibilityHandler handler);
    void setSourceMetadataChangedHandler(SourceMetadataChangedHandler handler);

    unsigned int tickIntervalMilliseconds() const override;
    PlaybackClockAction executeCommand(LegacyPlaybackCommand command) override;
    PlaybackClockAction seek(const PreviewSeekRequest &request) override;
    PlaybackClockAction synchronize() override;
    PlaybackClockAction advanceOneFrame() override;

private:
    MediaPlaybackPlan desiredPlaybackPlan() const;
    bool ensureSelectedSourceLoaded(const MediaPlaybackPlan &plan);
    bool isLoadedSourceStillRequested() const;
    bool isLoadedSourceStillSelected() const;
    bool ensureTimelineVideoLoaded(const MediaPlaybackPlan &plan,
                                   bool seekToTimelineFrame);
    bool isLoadedTimelineVideoStillActive() const;
    PlaybackClockAction synchronizeTimelinePlayback(
        bool seekToTimelineFrame = false);
    PlaybackClockAction synchronizeTimelinePlayback(
        const MediaPlaybackPlan &plan, bool seekToTimelineFrame);
    TimelineAudioPlaybackPlan desiredTimelineAudioPlan() const;
    bool ensureTimelineAudioLoaded(const TimelineAudioPlaybackPlan &plan,
                                   bool seekToTimelineFrame);
    void synchronizeTimelineAudio(const TimelineAudioPlaybackPlan &plan,
                                  bool seekToTimelineFrame);
    void stopTimelineAudioPlayback();
    void beginSilentFrameDecode(int targetSourceFrame);
    void finishSilentFirstFrameDecode();
    void cancelSilentFirstFrameDecode();
    // Runs on the GUI thread (queued there from QVideoSink::videoFrameChanged,
    // which Qt's FFmpeg-backed multimedia pipeline can emit from its own
    // decoder thread). frameGeneration is videoOutputGeneration_'s value at
    // the moment the frame was actually emitted, captured on whichever
    // thread that was -- see setVideoOutput()'s connection for why.
    void handleVideoFrameChanged(const QVideoFrame &frame, int frameGeneration);
    bool shouldMuteVideoTrackAudio() const;
    void applyPlaybackRate();
    void stopRealPlayback();
    void updateSessionFromPlayer();
    void setDecodedVideoVisible(bool visible);
    int playerDurationFrames() const;
    int playerPositionFrame() const;
    int unclampedPlayerPositionFrame() const;
    qint64 positionMillisecondsForFrame(int frame) const;

    EditorSession &session_;
    MediaLibrary &mediaLibrary_;
    SimulatedPlaybackBackend simulatedBackend_;
    // Members are destroyed in reverse declaration order. Keep the player
    // after its output so it disconnects before QAudioOutput is destroyed.
    QAudioOutput audioOutput_;
    QMediaPlayer player_;
    QAudioOutput timelineAudioOutput_;
    QMediaPlayer timelineAudioPlayer_;
    int loadedAssetId_ = 0;
    int loadedTimelineClipId_ = 0;
    bool loadedForTimeline_ = false;
    // A source ID alone is not enough while QMediaPlayer is replacing one
    // file with another: a queued position callback can still describe the
    // old file. This becomes true only after the new source is loaded.
    bool loadedSourceMediaReady_ = false;
    bool pauseAfterFirstVideoFrame_ = false;
    int silentDecodeTargetFrame_ = 0;
    // Incremented every time a new silent-decode-then-pause wait begins
    // (beginSilentFrameDecode()) -- a new source selection or a preroll
    // seek. A frame captured for an earlier generation must not be allowed
    // to act on whatever is loaded now (requirement: an old source frame
    // must not pause a newly selected source/clip). std::atomic because
    // QVideoSink::videoFrameChanged's own emitting thread reads it, while
    // only the GUI thread ever writes it.
    std::atomic<int> videoOutputGeneration_{0};
    // A GUI-thread-affine connection context purely for thread-affinity
    // purposes: QtMediaPlaybackBackend is not itself a QObject, and
    // QVideoSink::videoFrameChanged can be emitted from a Qt Multimedia
    // FFmpeg backend's own decoder thread, not the GUI thread that owns
    // player_. Connecting through this object makes Qt deliver the queued
    // callback on the GUI thread, where it is safe to call QMediaPlayer
    // methods (see setVideoOutput()).
    QObject videoFrameCallbackContext_;
    int pendingSourceSeekFrame_ = 0;
    int pendingTimelineSeekFrame_ = 0;
    bool hasPendingTimelineSeek_ = false;
    bool decodedVideoVisible_ = false;
    int loadedTimelineAudioAssetId_ = 0;
    int loadedTimelineAudioClipId_ = 0;
    int pendingTimelineAudioSourceFrame_ = 0;
    bool hasPendingTimelineAudioSeek_ = false;
    PreviewSeekRequestTracker seekRequests_;
    VideoVisibilityHandler videoVisibilityHandler_;
    SourceMetadataChangedHandler sourceMetadataChangedHandler_;
};
