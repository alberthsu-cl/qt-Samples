#include "MfcTimelineCanvas.h"

#include "DemoProject.h"
#include "MfcDoubleBufferedPaint.h"
#include "MfcEditorPaneBase.h"

#include <algorithm>
#include <utility>

namespace {

constexpr int kTimelineLeft = 76;
constexpr int kRulerHeight = 30;
constexpr int kTrackHeight = 66;
constexpr int kTimelineFramesPerScaleUnit = 300;
constexpr int kTimelineMaximumFrame = 600;
constexpr int kTimelinePixelsAt100Percent = 334;

} // namespace

BEGIN_MESSAGE_MAP(MfcTimelineCanvas, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

bool MfcTimelineCanvas::Create(CWnd *parent, UINT controlId)
{
    const CString windowClass = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(::GetStockObject(NULL_BRUSH)), nullptr);
    return CWnd::CreateEx(0, windowClass, _T(""),
                           WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                           CRect(0, 0, 1, 1), parent, controlId) != FALSE;
}

void MfcTimelineCanvas::setSelectedAssetIndex(int selectedAssetIndex)
{
    selectedAssetIndex_ = std::clamp(selectedAssetIndex, 0,
                                     static_cast<int>(demoAssets().size()) - 1);
    Invalidate(FALSE);
}

void MfcTimelineCanvas::setClipSettings(const ClipSettings &settings)
{
    clipSettings_ = settings;
    Invalidate(FALSE);
}

void MfcTimelineCanvas::setTimelineClipState(const TimelineClipState &state)
{
    timelineClipState_ = state;
    if (!isDraggingClip_)
        dragPreviewState_ = state;
    Invalidate(FALSE);
}

void MfcTimelineCanvas::setPlaybackState(const PlaybackState &state)
{
    playbackState_ = state;
    Invalidate(FALSE);
}

void MfcTimelineCanvas::setViewState(const TimelineViewState &state)
{
    viewState_ = state;
    Invalidate(FALSE);
}

void MfcTimelineCanvas::setSeekHandler(SeekHandler handler)
{
    seekHandler_ = std::move(handler);
}

void MfcTimelineCanvas::setTimelineClipEditedHandler(TimelineClipEditedHandler handler)
{
    timelineClipEditedHandler_ = std::move(handler);
}

void MfcTimelineCanvas::OnLButtonDown(UINT flags, CPoint point)
{
    if (point.y < kRulerHeight && seekHandler_)
        seekHandler_(frameAtRulerX(point.x));
    else if (timelineClipRect().PtInRect(point)) {
        isDraggingClip_ = true;
        dragPreviewState_ = timelineClipState_;
        dragFrameOffset_ = frameAtTimelineX(point.x) - dragPreviewState_.startFrame;
        SetCapture();
    }

    CWnd::OnLButtonDown(flags, point);
}

void MfcTimelineCanvas::OnMouseMove(UINT flags, CPoint point)
{
    if (isDraggingClip_ && GetCapture() == this) {
        dragPreviewState_.startFrame = std::clamp(
            frameAtTimelineX(point.x) - dragFrameOffset_, 0,
            kTimelineMaximumFrame - dragPreviewState_.durationFrames);
        Invalidate(FALSE);
    }

    CWnd::OnMouseMove(flags, point);
}

void MfcTimelineCanvas::OnLButtonUp(UINT flags, CPoint point)
{
    if (isDraggingClip_) {
        dragPreviewState_.startFrame = std::clamp(
            frameAtTimelineX(point.x) - dragFrameOffset_, 0,
            kTimelineMaximumFrame - dragPreviewState_.durationFrames);
        ReleaseCapture();
        isDraggingClip_ = false;

        if (timelineClipEditedHandler_)
            timelineClipEditedHandler_(dragPreviewState_);
        Invalidate(FALSE);
    }

    CWnd::OnLButtonUp(flags, point);
}

int MfcTimelineCanvas::frameAtRulerX(int x) const
{
    // The same zoom-aware width is used by OnPaint(). Keep this conversion in
    // the canvas: it owns pixel coordinates, while MainFrame owns frame state.
    const int clipWidth = std::max(1,
        kTimelinePixelsAt100Percent * viewState_.zoomPercent / 100);
    const int relativeX = std::clamp(x - kTimelineLeft, 0, clipWidth);
    return std::clamp(relativeX * kTimelineFramesPerScaleUnit / clipWidth,
                      0, kTimelineFramesPerScaleUnit - 1);
}

int MfcTimelineCanvas::frameAtTimelineX(int x) const
{
    const int pixelsPerScaleUnit = std::max(1,
        kTimelinePixelsAt100Percent * viewState_.zoomPercent / 100);
    return std::clamp((x - kTimelineLeft) * kTimelineFramesPerScaleUnit
                          / pixelsPerScaleUnit,
                      0, kTimelineMaximumFrame);
}

CRect MfcTimelineCanvas::timelineClipRect() const
{
    const TimelineClipState &clipState = isDraggingClip_ ? dragPreviewState_ : timelineClipState_;
    const int pixelsPerScaleUnit = std::max(1,
        kTimelinePixelsAt100Percent * viewState_.zoomPercent / 100);
    const int clipLeft = kTimelineLeft + clipState.startFrame * pixelsPerScaleUnit
        / kTimelineFramesPerScaleUnit;
    const int clipWidth = std::max(1, clipState.durationFrames * pixelsPerScaleUnit
        / kTimelineFramesPerScaleUnit);
    return CRect(clipLeft, kRulerHeight + 5, clipLeft + clipWidth,
                 kRulerHeight + kTrackHeight - 5);
}

void MfcTimelineCanvas::OnPaint()
{
    MfcDoubleBufferedPaint paint(this);
    CDC &deviceContext = paint.deviceContext();
    CRect clientRect;
    GetClientRect(&clientRect);

    deviceContext.FillSolidRect(clientRect, EditorUi::kPanelBackground);
    deviceContext.Draw3dRect(clientRect, EditorUi::kPanelBorder, EditorUi::kPanelBorder);

    const int rulerTop = 0;
    const int trackTop = rulerTop + kRulerHeight;
    const int audioTrackTop = trackTop + kTrackHeight + 8;
    deviceContext.FillSolidRect(0, rulerTop, clientRect.Width(), kRulerHeight, RGB(28, 30, 35));
    deviceContext.FillSolidRect(0, trackTop, clientRect.Width(), kTrackHeight, RGB(43, 46, 54));
    deviceContext.FillSolidRect(0, audioTrackTop, clientRect.Width(), kTrackHeight, RGB(38, 41, 48));

    const int tickSpacing = std::max(30, 80 * viewState_.zoomPercent / 100);
    for (int x = 110; x < clientRect.Width(); x += tickSpacing) {
        deviceContext.FillSolidRect(x, rulerTop + 18, 1, 12, EditorUi::kSecondaryText);
        CString timeLabel;
        timeLabel.Format(_T("%d"), (x - 110) * 10 / tickSpacing);
        deviceContext.SetBkMode(TRANSPARENT);
        deviceContext.SetTextColor(EditorUi::kSecondaryText);
        deviceContext.DrawText(timeLabel, CRect(x + 3, rulerTop + 2, x + 53, rulerTop + 18),
                               DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    deviceContext.SetBkMode(TRANSPARENT);
    deviceContext.SetTextColor(EditorUi::kSecondaryText);
    deviceContext.DrawText(_T("V1"), CRect(12, trackTop, 60, trackTop + kTrackHeight),
                           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    deviceContext.DrawText(_T("A1"), CRect(12, audioTrackTop, 60, audioTrackTop + kTrackHeight),
                           DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const auto &asset = demoAssets()[selectedAssetIndex_];
    const CRect clipRect = timelineClipRect();
    const int clipWidth = clipRect.Width();
    const COLORREF fadedThumbnailColor = RGB(
        GetRValue(asset.thumbnailColor) * clipSettings_.opacityPercent / 100,
        GetGValue(asset.thumbnailColor) * clipSettings_.opacityPercent / 100,
        GetBValue(asset.thumbnailColor) * clipSettings_.opacityPercent / 100);
    deviceContext.FillSolidRect(clipRect, fadedThumbnailColor);
    deviceContext.Draw3dRect(clipRect, RGB(180, 220, 255), RGB(180, 220, 255));
    deviceContext.SetTextColor(RGB(255, 255, 255));
    deviceContext.DrawText(asset.name,
                           CRect(clipRect.left + 10, clipRect.top,
                                 clipRect.right - 10, clipRect.bottom),
                           DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (viewState_.isAudioTrackVisible) {
        deviceContext.FillSolidRect(clipRect.left, audioTrackTop + 16, clipWidth, kTrackHeight - 16,
                                    RGB(38, 114, 176));
    } else {
        deviceContext.SetTextColor(EditorUi::kSecondaryText);
        deviceContext.DrawText(_T("Audio track hidden"),
                               CRect(kTimelineLeft, audioTrackTop, clientRect.right - 12,
                                     audioTrackTop + kTrackHeight),
                               DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    const int pixelsPerScaleUnit = std::max(1,
        kTimelinePixelsAt100Percent * viewState_.zoomPercent / 100);
    const int playheadX = kTimelineLeft + playbackState_.currentFrame * pixelsPerScaleUnit
        / kTimelineFramesPerScaleUnit;
    deviceContext.FillSolidRect(playheadX, rulerTop, 2,
                                clientRect.bottom - rulerTop, RGB(240, 74, 74));
}

BOOL MfcTimelineCanvas::OnEraseBkgnd(CDC *)
{
    return TRUE;
}
