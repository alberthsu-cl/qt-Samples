#pragma once

#include "PlaybackBackend.h"
#include "TimelinePlaybackResolver.h"

#include <QAudioOutput>
#include <QMediaPlayer>

#include <functional>

class EditorSession;
class MediaLibrary;
class QVideoSink;
struct LibraryMediaAsset;

// Qt Multimedia playback for a selected source video/audio and the active
// video clip on the timeline. Timeline decisions remain in the framework-
// neutral TimelinePlaybackResolver; this class only loads, seeks, and plays
// the media that the resolver selects.
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
    PlaybackClockAction synchronize() override;
    PlaybackClockAction advanceOneFrame() override;

private:
    const LibraryMediaAsset *selectedRealSource() const;
    bool ensureSelectedSourceLoaded();
    bool isLoadedSourceStillSelected() const;
    std::optional<ResolvedTimelineMedia> selectedTimelineVideo() const;
    bool ensureTimelineVideoLoaded(bool seekToTimelineFrame);
    bool isLoadedTimelineVideoStillActive() const;
    PlaybackClockAction synchronizeTimelinePlayback();
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
    bool pauseAfterFirstSourceVideoFrame_ = false;
    bool decodedVideoVisible_ = false;
    VideoVisibilityHandler videoVisibilityHandler_;
    SourceMetadataChangedHandler sourceMetadataChangedHandler_;
};
