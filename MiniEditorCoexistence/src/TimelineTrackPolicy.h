#pragma once

#include "MediaKind.h"
#include "TimelineModel.h"

#include <vector>

// Framework-neutral placement rules for the editor's single V1 and A1 tracks.
// Ranges are half-open [start, end), so clips may touch but never overlap.
class TimelineTrackPolicy final
{
public:
    static bool canPlace(const std::vector<TimelineClip> &clips,
                         TimelineTrackType trackType,
                         const TimelineClipState &state,
                         int ignoredClipId = 0);

    static int nearestAvailableStart(const std::vector<TimelineClip> &clips,
                                     TimelineTrackType trackType,
                                     int desiredStartFrame,
                                     int durationFrames,
                                     int ignoredClipId = 0);

    static TimelineClipState constrainStartTrim(
        const std::vector<TimelineClip> &clips,
        const TimelineClip &editedClip,
        const TimelineClipState &proposedState,
        MediaKind mediaKind);
    static TimelineClipState constrainEndTrim(
        const std::vector<TimelineClip> &clips,
        const TimelineClip &editedClip,
        const TimelineClipState &proposedState);
};
