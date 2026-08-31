#include "WorkspaceLayout.h"

#include <algorithm>

namespace {

constexpr int kOuterMargin = 6;
constexpr int kPreviewTransportSeparatorHeight = 2;
// The transport panel now contains a framed control group. Its 30-pixel
// buttons, group border, and panel margins need a little more than the old
// 42-pixel slot, otherwise the top and bottom of the group are clipped.
constexpr int kTransportHeight = 52;
// The Qt toolbar has buttons plus vertical layout margins. Reserve enough
// physical MFC pixels at common Windows DPI scales so its native child window
// never grows down over the timeline ruler labels beneath it.
constexpr int kTimelineToolbarHeight = 54;
constexpr int kSplitterThickness = 6;
constexpr int kMinimumMediaLibraryWidth = 340;
constexpr int kMinimumPropertiesWidth = 310;
constexpr int kMinimumPreviewWidth = 320;
constexpr int kMinimumTopAreaHeight = 250;
constexpr int kMinimumTimelineHeight = 240;

WorkspaceRect rectFromEdges(int left, int top, int right, int bottom)
{
    return { left, top, std::max(0, right - left), std::max(0, bottom - top) };
}

} // namespace

WorkspaceLayoutState WorkspaceLayout::state() const
{
    return state_;
}

void WorkspaceLayout::setState(const WorkspaceLayoutState &state)
{
    state_ = state;
}

WorkspaceGeometry WorkspaceLayout::calculate(int clientWidth, int contentBottom)
{
    const int safeClientWidth = std::max(0, clientWidth);
    const int safeContentBottom = std::max(0, contentBottom);

    const int maximumTimelineHeight = std::max(kMinimumTimelineHeight,
        safeContentBottom - kOuterMargin - kSplitterThickness - kMinimumTopAreaHeight);
    state_.timelineHeight = std::clamp(state_.timelineHeight, kMinimumTimelineHeight,
                                       maximumTimelineHeight);
    const int timelineTop = safeContentBottom - state_.timelineHeight;
    const int timelineSplitterTop = timelineTop - kSplitterThickness;

    const int maximumMediaWidth = std::max(kMinimumMediaLibraryWidth,
        safeClientWidth - kOuterMargin * 2 - kSplitterThickness * 2
                        - kMinimumPropertiesWidth - kMinimumPreviewWidth);
    state_.mediaLibraryWidth = std::clamp(state_.mediaLibraryWidth,
                                          kMinimumMediaLibraryWidth, maximumMediaWidth);
    const int maximumPropertiesWidth = std::max(kMinimumPropertiesWidth,
        safeClientWidth - kOuterMargin * 2 - kSplitterThickness * 2
                        - state_.mediaLibraryWidth - kMinimumPreviewWidth);
    state_.propertiesWidth = std::clamp(state_.propertiesWidth,
                                        kMinimumPropertiesWidth, maximumPropertiesWidth);

    const int mediaRight = kOuterMargin + state_.mediaLibraryWidth;
    const int centerLeft = mediaRight + kSplitterThickness;
    const int propertiesLeft = safeClientWidth - kOuterMargin - state_.propertiesWidth;
    const int rightSplitterLeft = propertiesLeft - kSplitterThickness;
    const int topAreaBottom = timelineSplitterTop;
    const int canvasBottom = std::max(
        kOuterMargin,
        topAreaBottom - kTransportHeight - kPreviewTransportSeparatorHeight);

    WorkspaceGeometry geometry;
    geometry.mediaLibrary = rectFromEdges(kOuterMargin, kOuterMargin,
                                          mediaRight, topAreaBottom);
    geometry.previewCanvas = rectFromEdges(centerLeft, kOuterMargin,
                                           rightSplitterLeft, canvasBottom);
    geometry.transport = rectFromEdges(centerLeft,
                                       canvasBottom + kPreviewTransportSeparatorHeight,
                                       rightSplitterLeft, topAreaBottom);
    geometry.properties = rectFromEdges(propertiesLeft, kOuterMargin,
                                        safeClientWidth - kOuterMargin, topAreaBottom);
    geometry.timeline = rectFromEdges(kOuterMargin, timelineTop,
                                      safeClientWidth - kOuterMargin, safeContentBottom);
    geometry.timelineToolbar = rectFromEdges(kOuterMargin, timelineTop,
                                             safeClientWidth - kOuterMargin,
                                             timelineTop + kTimelineToolbarHeight);
    geometry.timelineCanvas = rectFromEdges(kOuterMargin,
                                            timelineTop + kTimelineToolbarHeight,
                                            safeClientWidth - kOuterMargin,
                                            safeContentBottom);
    geometry.leftSplitter = rectFromEdges(mediaRight, kOuterMargin,
                                          mediaRight + kSplitterThickness, topAreaBottom);
    geometry.rightSplitter = rectFromEdges(rightSplitterLeft, kOuterMargin,
                                           propertiesLeft, topAreaBottom);
    geometry.timelineSplitter = rectFromEdges(kOuterMargin, timelineSplitterTop,
                                              safeClientWidth - kOuterMargin, timelineTop);
    return geometry;
}

void WorkspaceLayout::moveLeftSplitter(int parentX)
{
    state_.mediaLibraryWidth = std::max(kMinimumMediaLibraryWidth, parentX - kOuterMargin);
}

void WorkspaceLayout::moveRightSplitter(int parentX, int clientWidth)
{
    state_.propertiesWidth = std::max(kMinimumPropertiesWidth,
        clientWidth - kOuterMargin - (parentX + kSplitterThickness));
}

void WorkspaceLayout::moveTimelineSplitter(int parentY, int contentBottom)
{
    state_.timelineHeight = std::max(kMinimumTimelineHeight,
        contentBottom - (parentY + kSplitterThickness));
}
