#include "TimelineAudioPlaybackPlan.h"

#include "EditorSession.h"
#include "MediaLibrary.h"
#include "TimelinePlaybackResolver.h"

bool TimelineAudioPlaybackPlan::hasAudio() const
{
    return mediaAssetId != 0 && timelineClipId != 0;
}

TimelineAudioPlaybackPlan TimelineAudioPlaybackPlanResolver::resolve(
    const EditorSession &session, const MediaLibrary &mediaLibrary)
{
    if (!session.isTimelineFocused()
        || !session.timelineViewState().isAudioTrackVisible) {
        return {};
    }

    const PlaybackState &playback = session.timelinePlaybackState();
    const ResolvedTimelineFrame frame = TimelinePlaybackResolver::resolve(
        session.timelineModel(), mediaLibrary, playback.currentFrame);
    if (!frame.audio || frame.audio->mediaKind != MediaKind::Audio)
        return {};

    return {
        frame.audio->mediaAssetId,
        frame.audio->clipId,
        frame.audio->sourceFrame,
        frame.audio->sourceDurationFrames,
        frame.audio->fadeGainPercent,
        playback.isPlaying,
        playback.isPaused
    };
}
