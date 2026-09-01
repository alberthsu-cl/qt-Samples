#include "TimelinePresentationStateResolver.h"

#include "EditorSession.h"

TimelinePresentationState TimelinePresentationStateResolver::resolve(
    const EditorSession &session, bool splitEnabled)
{
    TimelinePresentationState state;
    state.clips = session.timelineModel().clips();
    state.selectedClipId = session.selectedTimelineClipId();
    state.durationFrames = session.timelineModel().durationFrames();
    state.playback = session.timelinePlaybackState();
    state.view = session.timelineViewState();
    state.audioMix = session.timelineAudioMixState();
    state.splitEnabled = splitEnabled;
    return state;
}
