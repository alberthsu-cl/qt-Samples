#include "MfcTimelineCanvas.h"

#include "DemoProject.h"
#include "MfcEditorPaneBase.h"

#include <algorithm>

BEGIN_MESSAGE_MAP(MfcTimelineCanvas, CWnd)
    ON_WM_PAINT()
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
    Invalidate();
}

void MfcTimelineCanvas::setClipSettings(const ClipSettings &settings)
{
    clipSettings_ = settings;
    Invalidate();
}

void MfcTimelineCanvas::setPlaybackState(const PlaybackState &state)
{
    playbackState_ = state;
    Invalidate();
}

void MfcTimelineCanvas::setViewState(const TimelineViewState &state)
{
    viewState_ = state;
    Invalidate();
}

void MfcTimelineCanvas::OnPaint()
{
    CPaintDC deviceContext(this);
    CRect clientRect;
    GetClientRect(&clientRect);

    deviceContext.FillSolidRect(clientRect, EditorUi::kPanelBackground);
    deviceContext.Draw3dRect(clientRect, EditorUi::kPanelBorder, EditorUi::kPanelBorder);

    constexpr int kRulerHeight = 30;
    constexpr int kTrackHeight = 66;
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
    const int availableClipWidth = std::max(1, clientRect.Width() - 76);
    const int clipWidth = std::min(availableClipWidth,
        334 * clipSettings_.scalePercent * viewState_.zoomPercent / 10000);
    const CRect clipRect(76, trackTop + 5, 76 + clipWidth, trackTop + kTrackHeight - 5);
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
        deviceContext.FillSolidRect(76, audioTrackTop + 16, clipWidth, kTrackHeight - 16,
                                    RGB(38, 114, 176));
    } else {
        deviceContext.SetTextColor(EditorUi::kSecondaryText);
        deviceContext.DrawText(_T("Audio track hidden"),
                               CRect(76, audioTrackTop, clientRect.right - 12,
                                     audioTrackTop + kTrackHeight),
                               DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    const int playheadX = 76 + clipWidth * playbackState_.currentFrame / 300;
    deviceContext.FillSolidRect(playheadX, rulerTop, 2,
                                clientRect.bottom - rulerTop, RGB(240, 74, 74));
}
