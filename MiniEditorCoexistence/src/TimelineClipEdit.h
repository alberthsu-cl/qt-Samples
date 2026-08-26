#pragma once

#include "MediaKind.h"
#include "ProjectState.h"

struct TimelineTrimContext {
    MediaKind mediaKind = MediaKind::Video;
    int sourceDurationFrames = 1;
};

// Framework-neutral timeline editing calculations. The canvas supplies the
// frame under the mouse; this class produces a valid provisional clip state.
class TimelineClipEdit final
{
public:
    static TimelineClipState moveTo(const TimelineClipState &original,
                                    int startFrame);
    static TimelineClipState trimStartTo(const TimelineClipState &original,
                                         int startFrame,
                                         const TimelineTrimContext &context);
    static TimelineClipState trimEndTo(const TimelineClipState &original,
                                       int endFrame,
                                       const TimelineTrimContext &context);
};
