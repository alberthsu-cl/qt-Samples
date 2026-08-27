#include "TimelinePresentationStateResolver.h"

#include "EditorSession.h"

TimelinePresentationState TimelinePresentationStateResolver::resolve(
    const EditorSession &session, bool splitEnabled)
{
    TimelinePresentationState state;
    state.clips = session.timelineModel().clips();
    state.selectedClipId = session.selectedTimelineClipId();
    state.durationFrames = session.timelineModel().durationFrames();
    state.playback = session.playbackState();
    state.playback.currentFrame = session.timelinePlayheadFrame();
    if (!session.isTimelineFocused()) {
        state.playback.isPlaying = false;
        state.playback.isPaused = false;
    }
    state.view = session.timelineViewState();
    state.splitEnabled = splitEnabled;
    return state;
}
