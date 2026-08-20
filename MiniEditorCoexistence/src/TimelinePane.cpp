#include "TimelinePane.h"

#include "DemoProject.h"

#include <algorithm>

bool TimelinePane::Create(CWnd *parent, UINT controlId)
{
    return createPane(parent, controlId);
}

CString TimelinePane::paneTitle() const
{
    return _T("Timeline");
}

void TimelinePane::drawContent(CDC &deviceContext, const CRect &clientRect) const
{
    const int rulerTop = EditorUi::kHeaderHeight;
    const int trackTop = rulerTop + 30;
    const int trackHeight = 66;
    deviceContext.FillSolidRect(0, rulerTop, clientRect.Width(), 30, RGB(28, 30, 35));
    deviceContext.FillSolidRect(0, trackTop, clientRect.Width(), trackHeight, RGB(43, 46, 54));
    deviceContext.FillSolidRect(0, trackTop + trackHeight + 8,
                                clientRect.Width(), trackHeight, RGB(38, 41, 48));

    for (int x = 110; x < clientRect.Width(); x += 80) {
        deviceContext.FillSolidRect(x, rulerTop + 18, 1, 12, EditorUi::kSecondaryText);
        CString timeLabel;
        timeLabel.Format(_T("%d"), (x - 110) / 8);
        drawText(deviceContext, timeLabel,
                 CRect(x + 3, rulerTop + 2, x + 53, rulerTop + 18),
                 EditorUi::kSecondaryText);
    }

    drawText(deviceContext, _T("V1"), CRect(12, trackTop, 60, trackTop + trackHeight),
             EditorUi::kSecondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    drawText(deviceContext, _T("A1"), CRect(12, trackTop + trackHeight + 8,
                                              60, trackTop + trackHeight * 2 + 8),
             EditorUi::kSecondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const auto &asset = demoAssets()[selectedAssetIndex()];
    const ClipSettings &settings = clipSettings();
    const int availableClipWidth = std::max(1, static_cast<int>(clientRect.right) - 76);
    const int clipWidth = std::min(availableClipWidth,
                                   334 * settings.scalePercent / 100);
    const CRect clipRect(76, trackTop + 5, 76 + clipWidth, trackTop + trackHeight - 5);
    const COLORREF fadedThumbnailColor = RGB(
        GetRValue(asset.thumbnailColor) * settings.opacityPercent / 100,
        GetGValue(asset.thumbnailColor) * settings.opacityPercent / 100,
        GetBValue(asset.thumbnailColor) * settings.opacityPercent / 100);
    deviceContext.FillSolidRect(clipRect, fadedThumbnailColor);
    deviceContext.Draw3dRect(clipRect, RGB(180, 220, 255), RGB(180, 220, 255));
    drawText(deviceContext, asset.name,
             CRect(clipRect.left + 10, clipRect.top, clipRect.right - 10, clipRect.bottom),
             RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    deviceContext.FillSolidRect(76, trackTop + trackHeight + 16, clipWidth, trackHeight - 16,
                                RGB(38, 114, 176));
    deviceContext.FillSolidRect(76, rulerTop, 2, clientRect.bottom - rulerTop, RGB(240, 74, 74));
}
