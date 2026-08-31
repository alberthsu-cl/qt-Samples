#pragma once

#include "MediaPlaybackPlan.h"
#include "PlaybackBackend.h"

#include <QAudioOutput>
#include <QMediaPlayer>

#include <functional>

class EditorSession;
class MediaLibrary;
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
    PlaybackClockAction executeCommand(PlaybackCommand command) override;
    PlaybackClockAction seekToCurrentFrame() override;
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
    void beginSilentFirstFrameDecode();
    void finishSilentFirstFrameDecode();
    void cancelSilentFirstFrameDecode();
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
    int loadedAssetId_ = 0;
    int loadedTimelineClipId_ = 0;
    bool loadedForTimeline_ = false;
    // A source ID alone is not enough while QMediaPlayer is replacing one
    // file with another: a queued position callback can still describe the
    // old file. This becomes true only after the new source is loaded.
    bool loadedSourceMediaReady_ = false;
    bool pauseAfterFirstVideoFrame_ = false;
    int pendingSourceSeekFrame_ = 0;
    int pendingTimelineSeekFrame_ = 0;
    bool hasPendingTimelineSeek_ = false;
    bool decodedVideoVisible_ = false;
    VideoVisibilityHandler videoVisibilityHandler_;
    SourceMetadataChangedHandler sourceMetadataChangedHandler_;
};
