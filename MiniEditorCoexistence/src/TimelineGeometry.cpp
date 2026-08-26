#include "TimelineGeometry.h"

#include <algorithm>

bool TimelineRectangle::contains(const TimelinePoint &point) const
{
    return point.x >= left && point.x < left + width
        && point.y >= top && point.y < top + height;
}

TimelineGeometry::TimelineGeometry(int zoomPercent, int durationFrames)
    : zoomPercent_(std::max(1, zoomPercent))
    , durationFrames_(std::max(1, durationFrames))
{
}

int TimelineGeometry::pixelsPerScaleUnit() const
{
    return std::max(1, kPixelsAt100Percent * zoomPercent_ / 100);
}

int TimelineGeometry::xForFrame(int frame) const
{
    return kTimelineLeft + std::max(0, frame) * pixelsPerScaleUnit()
        / kFramesPerScaleUnit;
}

int TimelineGeometry::frameAtXUnclamped(int x) const
{
    return std::max(0, (x - kTimelineLeft) * kFramesPerScaleUnit
                           / pixelsPerScaleUnit());
}

int TimelineGeometry::frameAtX(int x) const
{
    return std::min(frameAtXUnclamped(x), durationFrames_);
}

int TimelineGeometry::rulerFrameAtX(int x) const
{
    return std::min(frameAtX(x), durationFrames_ - 1);
}

int TimelineGeometry::contentWidth() const
{
    return xForFrame(durationFrames_) + kTrailingMargin;
}

TimelineRectangle TimelineGeometry::trackRectangle(TimelineTrackType trackType,
                                                   int canvasWidth) const
{
    const int top = trackType == TimelineTrackType::Audio
        ? kRulerHeight + kTrackHeight + kTrackGap : kRulerHeight;
    return { 0, top, std::max(0, canvasWidth), kTrackHeight };
}

TimelineRectangle TimelineGeometry::clipRectangle(
    TimelineTrackType trackType, const TimelineClipState &state) const
{
    const TimelineRectangle track = trackRectangle(trackType, 0);
    const int width = std::max(1, std::max(1, state.durationFrames)
        * pixelsPerScaleUnit() / kFramesPerScaleUnit);
    return { xForFrame(state.startFrame),
             track.top + kClipVerticalMargin,
             width,
             kTrackHeight - kClipVerticalMargin * 2 };
}

TimelineRectangle TimelineGeometry::clipRectangle(const TimelineClip &clip) const
{
    return clipRectangle(clip.trackType, clip.state);
}

const TimelineClip *TimelineGeometry::topmostClipAt(
    const std::vector<TimelineClip> &clips, const TimelinePoint &point) const
{
    // Painting uses insertion order. Reverse hit-testing therefore returns
    // the same later clip that the user sees on top in an overlap.
    for (auto iterator = clips.rbegin(); iterator != clips.rend(); ++iterator) {
        if (clipRectangle(*iterator).contains(point))
            return &*iterator;
    }
    return nullptr;
}

TimelineClipHit TimelineGeometry::hitTestClip(
    const std::vector<TimelineClip> &clips, const TimelinePoint &point,
    int selectedClipId, int trimHandleWidth) const
{
    const TimelineClip *clip = topmostClipAt(clips, point);
    if (clip == nullptr)
        return {};

    if (clip->id != selectedClipId || trimHandleWidth <= 0)
        return { clip, TimelineClipHitRegion::Body };

    const TimelineRectangle rectangle = clipRectangle(*clip);
    const int distanceFromStart = point.x - rectangle.left;
    const int distanceFromEnd = rectangle.left + rectangle.width - 1 - point.x;
    if (std::min(distanceFromStart, distanceFromEnd) >= trimHandleWidth)
        return { clip, TimelineClipHitRegion::Body };

    return { clip, distanceFromStart <= distanceFromEnd
                       ? TimelineClipHitRegion::TrimStart
                       : TimelineClipHitRegion::TrimEnd };
}
