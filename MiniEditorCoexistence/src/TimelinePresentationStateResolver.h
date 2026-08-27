#pragma once

#include "ProjectState.h"
#include "TimelineModel.h"

#include <vector>

class EditorSession;

// Everything the timeline canvas and toolbar need for one coherent refresh.
// It deliberately contains no QWidget, HWND, signal, or command ID.
struct TimelinePresentationState {
    std::vector<TimelineClip> clips;
    int selectedClipId = 0;
    int durationFrames = 0;
    PlaybackState playback;
    TimelineViewState view;
    bool splitEnabled = false;
};

class TimelinePresentationStateResolver final
{
public:
    static TimelinePresentationState resolve(const EditorSession &session,
                                             bool splitEnabled);
};
