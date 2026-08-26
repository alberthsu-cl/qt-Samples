#pragma once

#include "TimelineModel.h"

#include <vector>

struct TimelinePoint {
    int x = 0;
    int y = 0;
};

// A small UI-framework-neutral rectangle. QtTimelineCanvas converts it to
// QRect, while the MFC fallback converts it to CRect.
struct TimelineRectangle {
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;

    bool contains(const TimelinePoint &point) const;
};

enum class TimelineClipHitRegion {
    None,
    Body,
    TrimStart,
    TrimEnd
};

struct TimelineClipHit {
    const TimelineClip *clip = nullptr;
    TimelineClipHitRegion region = TimelineClipHitRegion::None;
};

// Owns the coordinate policy for the fixed V1/A1 learning timeline. It knows
// nothing about QWidget, QPainter, CWnd, or CDC.
class TimelineGeometry final
{
public:
    static constexpr int kTimelineLeft = 76;
    static constexpr int kRulerHeight = 30;
    static constexpr int kTrackHeight = 66;
    static constexpr int kTrackGap = 8;
    static constexpr int kBottomMargin = 8;
    static constexpr int kClipVerticalMargin = 5;
    static constexpr int kFramesPerScaleUnit = 300;
    static constexpr int kPixelsAt100Percent = 334;
    static constexpr int kTrailingMargin = 24;
    static constexpr int kCanvasHeight =
        kRulerHeight + kTrackHeight * 2 + kTrackGap + kBottomMargin;

    TimelineGeometry(int zoomPercent, int durationFrames);

    int pixelsPerScaleUnit() const;
    int xForFrame(int frame) const;
    int frameAtX(int x) const;
    int rulerFrameAtX(int x) const;
    int contentWidth() const;

    TimelineRectangle trackRectangle(TimelineTrackType trackType,
                                     int canvasWidth) const;
    TimelineRectangle clipRectangle(TimelineTrackType trackType,
                                    const TimelineClipState &state) const;
    TimelineRectangle clipRectangle(const TimelineClip &clip) const;

    const TimelineClip *topmostClipAt(const std::vector<TimelineClip> &clips,
                                      const TimelinePoint &point) const;
    TimelineClipHit hitTestClip(const std::vector<TimelineClip> &clips,
                                const TimelinePoint &point,
                                int selectedClipId,
                                int trimHandleWidth) const;

private:
    int zoomPercent_ = 100;
    int durationFrames_ = TimelineModel::kMinimumDurationFrames;
};
