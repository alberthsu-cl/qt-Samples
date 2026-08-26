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

int TimelineTrackPolicy::magneticallySnappedStart(
    const std::vector<TimelineClip> &clips, TimelineTrackType trackType,
    int desiredStartFrame, int durationFrames, int ignoredClipId,
    int snapToleranceFrames)
{
    const int desired = std::max(0, desiredStartFrame);
    const int duration = std::max(1, durationFrames);
    const TimelineClipState desiredState{ desired, duration, 0 };

    // An overlapping proposal must first move to a legal gap, regardless of
    // snap tolerance. Magnetism is only a UI aid; non-overlap is an invariant.
    if (!canPlace(clips, trackType, desiredState, ignoredClipId)) {
        return nearestAvailableStart(clips, trackType, desired, duration,
                                     ignoredClipId);
    }

    const int tolerance = std::max(0, snapToleranceFrames);
    int bestStart = desired;
    int bestDistance = tolerance + 1;
    const auto consider = [&](int candidate, int &currentBest,
                              int &currentDistance) {
        const TimelineClipState candidateState{ candidate, duration, 0 };
        if (!canPlace(clips, trackType, candidateState, ignoredClipId))
            return;
        const int distance = std::abs(candidate - desired);
        if (distance <= tolerance
            && (distance < currentDistance
                || (distance == currentDistance && candidate < currentBest))) {
            currentBest = candidate;
            currentDistance = distance;
        }
    };

    consider(0, bestStart, bestDistance);
    for (const OccupiedRange &range : occupiedRanges(clips, trackType,
                                                      ignoredClipId)) {
        // Align either the moving clip's left edge after an existing clip or
        // its right edge before one.
        consider(range.end, bestStart, bestDistance);
        consider(range.start - duration, bestStart, bestDistance);
    }
    return bestStart;
}

int TimelineTrackPolicy::rippleInsertionStart(
    const std::vector<TimelineClip> &clips, TimelineTrackType trackType,
    int desiredStartFrame, int snapToleranceFrames)
{
    const int desired = std::max(0, desiredStartFrame);
    const int tolerance = std::max(0, snapToleranceFrames);
    int bestStart = desired;
    int bestDistance = tolerance + 1;
    const auto consider = [&](int candidate) {
        const int distance = std::abs(candidate - desired);
        if (distance <= tolerance
            && (distance < bestDistance
                || (distance == bestDistance && candidate < bestStart))) {
            bestStart = candidate;
            bestDistance = distance;
        }
    };

    consider(0);
    for (const OccupiedRange &range : occupiedRanges(clips, trackType, 0)) {
        // Dropping inside a clip would require splitting it. This learning
        // sample instead chooses the nearer existing edit boundary.
        if (desired > range.start && desired < range.end) {
            const int distanceToStart = desired - range.start;
            const int distanceToEnd = range.end - desired;
            return distanceToStart <= distanceToEnd ? range.start : range.end;
        }
        consider(range.start);
        consider(range.end);
    }
    return bestStart;
}

int TimelineTrackPolicy::rippleMoveStart(
    const std::vector<TimelineClip> &clips, int movedClipId,
    int desiredStartFrame, int snapToleranceFrames)
{
    const auto moved = std::find_if(clips.begin(), clips.end(),
        [movedClipId](const TimelineClip &clip) {
            return clip.id == movedClipId;
        });
    if (moved == clips.end())
        return std::max(0, desiredStartFrame);

    std::vector<TimelineClip> collapsed;
    collapsed.reserve(clips.size() - 1);
    const int movedEnd = moved->state.startFrame + moved->state.durationFrames;
    for (const TimelineClip &clip : clips) {
        if (clip.id == movedClipId)
            continue;
        TimelineClip remaining = clip;
        if (remaining.trackType == moved->trackType
            && remaining.state.startFrame >= movedEnd) {
            remaining.state.startFrame -= moved->state.durationFrames;
        }
        collapsed.push_back(remaining);
    }

    // Mouse coordinates come from the original timeline. A destination to
    // the right moves left by the removed clip duration after its old gap is
    // closed.
    int collapsedDesired = std::max(0, desiredStartFrame);
    if (collapsedDesired > moved->state.startFrame)
        collapsedDesired = std::max(0, collapsedDesired - moved->state.durationFrames);
    return rippleInsertionStart(collapsed, moved->trackType, collapsedDesired,
                                snapToleranceFrames);
}

TimelineClipState TimelineTrackPolicy::constrainStartTrim(
    const std::vector<TimelineClip> &clips, const TimelineClip &editedClip,
    const TimelineClipState &proposedState, MediaKind mediaKind,
    int snapToleranceFrames)
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

    const bool crossesBoundary = proposedState.startFrame < minimumStart;
    const bool isNearBoundary = std::abs(proposedState.startFrame - minimumStart)
        <= std::max(0, snapToleranceFrames);
    if (!crossesBoundary && !isNearBoundary)
        return proposedState;

    TimelineClipState constrained = proposedState;
    const int removedFrames = minimumStart - proposedState.startFrame;
    const int sourceInFrame = mediaKind == MediaKind::Image
        ? 0 : proposedState.sourceInFrame + removedFrames;
    if (sourceInFrame < 0)
        return proposedState;
    constrained.startFrame = minimumStart;
    constrained.durationFrames = originalEnd - minimumStart;
    constrained.sourceInFrame = sourceInFrame;
    return constrained;
}

TimelineClipState TimelineTrackPolicy::constrainEndTrim(
    const std::vector<TimelineClip> &clips, const TimelineClip &editedClip,
    const TimelineClipState &proposedState, int latestAllowedEndFrame,
    int snapToleranceFrames)
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
    const bool crossesBoundary = proposedEnd > maximumEnd;
    const bool isNearBoundary = maximumEnd != std::numeric_limits<int>::max()
        && std::abs(proposedEnd - maximumEnd)
            <= std::max(0, snapToleranceFrames);
    if (!crossesBoundary && !isNearBoundary)
        return proposedState;

    if (maximumEnd > latestAllowedEndFrame)
        return proposedState;

    TimelineClipState constrained = proposedState;
    constrained.durationFrames = maximumEnd - proposedState.startFrame;
    return constrained;
}
