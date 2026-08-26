#include "TimelineTrackPolicy.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace {

struct OccupiedRange {
    int start;
    int end;
};

std::vector<OccupiedRange> occupiedRanges(const std::vector<TimelineClip> &clips,
                                          TimelineTrackType trackType,
                                          int ignoredClipId)
{
    std::vector<OccupiedRange> ranges;
    for (const TimelineClip &clip : clips) {
        if (clip.trackType != trackType || clip.id == ignoredClipId)
            continue;
        ranges.push_back({ clip.state.startFrame,
                           clip.state.startFrame + clip.state.durationFrames });
    }
    std::sort(ranges.begin(), ranges.end(),
        [](const OccupiedRange &left, const OccupiedRange &right) {
            return left.start < right.start;
        });
    return ranges;
}

} // namespace

bool TimelineTrackPolicy::canPlace(const std::vector<TimelineClip> &clips,
                                   TimelineTrackType trackType,
                                   const TimelineClipState &state,
                                   int ignoredClipId)
{
    if (state.startFrame < 0 || state.durationFrames <= 0 || state.sourceInFrame < 0)
        return false;

    const int proposedEnd = state.startFrame + state.durationFrames;
    for (const TimelineClip &clip : clips) {
        if (clip.trackType != trackType || clip.id == ignoredClipId)
            continue;
        const int clipEnd = clip.state.startFrame + clip.state.durationFrames;
        if (state.startFrame < clipEnd && clip.state.startFrame < proposedEnd)
            return false;
    }
    return true;
}

int TimelineTrackPolicy::nearestAvailableStart(
    const std::vector<TimelineClip> &clips, TimelineTrackType trackType,
    int desiredStartFrame, int durationFrames, int ignoredClipId)
{
    const int desired = std::max(0, desiredStartFrame);
    const int duration = std::max(1, durationFrames);
    const std::vector<OccupiedRange> ranges = occupiedRanges(
        clips, trackType, ignoredClipId);

    int bestStart = 0;
    long long bestDistance = std::numeric_limits<long long>::max();
    const auto considerGap = [&](int firstStart, int lastStart,
                                 int &currentBest, long long &currentDistance) {
        if (lastStart < firstStart)
            return;
        const int candidate = std::clamp(desired, firstStart, lastStart);
        const long long distance = std::llabs(
            static_cast<long long>(candidate) - desired);
        if (distance < currentDistance
            || (distance == currentDistance && candidate < currentBest)) {
            currentBest = candidate;
            currentDistance = distance;
        }
    };

    int previousEnd = 0;
    for (const OccupiedRange &range : ranges) {
        considerGap(previousEnd, range.start - duration, bestStart, bestDistance);
        previousEnd = std::max(previousEnd, range.end);
    }

    // The final gap has no right boundary. The desired position is valid when
    // it lies after the occupied content; otherwise snap to the final edge.
    const int trailingStart = std::max(previousEnd, desired);
    considerGap(trailingStart, trailingStart, bestStart, bestDistance);
    return bestStart;
}

TimelineClipState TimelineTrackPolicy::constrainStartTrim(
    const std::vector<TimelineClip> &clips, const TimelineClip &editedClip,
    const TimelineClipState &proposedState, MediaKind mediaKind)
{
    const int originalEnd = editedClip.state.startFrame
        + editedClip.state.durationFrames;
    int minimumStart = 0;
    for (const TimelineClip &clip : clips) {
        if (clip.trackType != editedClip.trackType || clip.id == editedClip.id)
            continue;
        const int clipEnd = clip.state.startFrame + clip.state.durationFrames;
        if (clipEnd <= editedClip.state.startFrame)
            minimumStart = std::max(minimumStart, clipEnd);
    }

    if (proposedState.startFrame >= minimumStart)
        return proposedState;

    TimelineClipState constrained = proposedState;
    const int removedFrames = minimumStart - proposedState.startFrame;
    constrained.startFrame = minimumStart;
    constrained.durationFrames = originalEnd - minimumStart;
    constrained.sourceInFrame = mediaKind == MediaKind::Image
        ? 0 : proposedState.sourceInFrame + removedFrames;
    return constrained;
}

TimelineClipState TimelineTrackPolicy::constrainEndTrim(
    const std::vector<TimelineClip> &clips, const TimelineClip &editedClip,
    const TimelineClipState &proposedState)
{
    int maximumEnd = std::numeric_limits<int>::max();
    const int originalEnd = editedClip.state.startFrame
        + editedClip.state.durationFrames;
    for (const TimelineClip &clip : clips) {
        if (clip.trackType != editedClip.trackType || clip.id == editedClip.id)
            continue;
        if (clip.state.startFrame >= originalEnd)
            maximumEnd = std::min(maximumEnd, clip.state.startFrame);
    }

    const int proposedEnd = proposedState.startFrame + proposedState.durationFrames;
    if (proposedEnd <= maximumEnd)
        return proposedState;

    TimelineClipState constrained = proposedState;
    constrained.durationFrames = maximumEnd - proposedState.startFrame;
    return constrained;
}
