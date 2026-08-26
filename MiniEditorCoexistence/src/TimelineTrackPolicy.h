#pragma once

#include "MediaKind.h"
#include "TimelineModel.h"

#include <limits>
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

    static int magneticallySnappedStart(
        const std::vector<TimelineClip> &clips,
        TimelineTrackType trackType,
        int desiredStartFrame,
        int durationFrames,
        int ignoredClipId,
        int snapToleranceFrames);

    static int rippleInsertionStart(
        const std::vector<TimelineClip> &clips,
        TimelineTrackType trackType,
        int desiredStartFrame,
        int snapToleranceFrames);

    static int rippleMoveStart(
        const std::vector<TimelineClip> &clips,
        int movedClipId,
        int desiredStartFrame,
        int snapToleranceFrames);

    static TimelineClipState constrainStartTrim(
        const std::vector<TimelineClip> &clips,
        const TimelineClip &editedClip,
        const TimelineClipState &proposedState,
        MediaKind mediaKind,
        int snapToleranceFrames = 0);
    static TimelineClipState constrainEndTrim(
        const std::vector<TimelineClip> &clips,
        const TimelineClip &editedClip,
        const TimelineClipState &proposedState,
        int latestAllowedEndFrame = std::numeric_limits<int>::max(),
        int snapToleranceFrames = 0);
};
