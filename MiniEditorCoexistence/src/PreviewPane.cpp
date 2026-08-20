#include "PreviewPane.h"

#include "DemoProject.h"

#include <algorithm>

bool PreviewPane::Create(CWnd *parent, UINT controlId)
{
    return createPane(parent, controlId);
}

CString PreviewPane::paneTitle() const
{
    return _T("Preview");
}

void PreviewPane::drawContent(CDC &deviceContext, const CRect &clientRect) const
{
    const auto &asset = demoAssets()[selectedAssetIndex()];
    const ClipSettings &settings = clipSettings();
    const CRect availableRect(20, EditorUi::kHeaderHeight + 18,
                              clientRect.right - 20, clientRect.bottom - 66);
    const int availableWidth = availableRect.Width();
    const int availableHeight = availableRect.Height();
    const int baseVideoHeight = std::min(availableHeight, availableWidth * 9 / 16);
    const int videoHeight = std::max(1, std::min(availableHeight,
        baseVideoHeight * settings.scalePercent / 100));
    const int videoWidth = std::max(1, std::min(availableWidth, videoHeight * 16 / 9));

    int videoLeft = availableRect.left + (availableWidth - videoWidth) / 2;
    int videoTop = availableRect.top + (availableHeight - videoHeight) / 2;
    switch (settings.position) {
    case ClipPosition::TopLeft:
        videoLeft = availableRect.left;
        videoTop = availableRect.top;
        break;
    case ClipPosition::TopRight:
        videoLeft = availableRect.right - videoWidth;
        videoTop = availableRect.top;
        break;
    case ClipPosition::BottomLeft:
        videoLeft = availableRect.left;
        videoTop = availableRect.bottom - videoHeight;
        break;
    case ClipPosition::BottomRight:
        videoLeft = availableRect.right - videoWidth;
        videoTop = availableRect.bottom - videoHeight;
        break;
    case ClipPosition::Center:
        break;
    }
    const CRect videoRect(videoLeft, videoTop, videoLeft + videoWidth, videoTop + videoHeight);

    deviceContext.FillSolidRect(availableRect, EditorUi::kCanvasBackground);
    const COLORREF fadedThumbnailColor = RGB(
        GetRValue(asset.thumbnailColor) * settings.opacityPercent / 100,
        GetGValue(asset.thumbnailColor) * settings.opacityPercent / 100,
        GetBValue(asset.thumbnailColor) * settings.opacityPercent / 100);
    deviceContext.FillSolidRect(videoRect, fadedThumbnailColor);
    deviceContext.Draw3dRect(videoRect, RGB(220, 220, 220), RGB(220, 220, 220));

    CRect titleRect(videoRect.left + 12, videoRect.bottom - 38,
                    videoRect.right - 12, videoRect.bottom - 12);
    drawText(deviceContext, asset.name, titleRect, RGB(255, 255, 255),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    CString settingsText;
    settingsText.Format(_T("Opacity %d%%  |  Scale %d%%  |  %s"),
                        settings.opacityPercent, settings.scalePercent,
                        clipPositionDisplayName(settings.position));
    drawText(deviceContext, settingsText,
             CRect(availableRect.left, availableRect.bottom - 24,
                   availableRect.right, availableRect.bottom - 2),
             EditorUi::kSecondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    CRect controlsRect(20, clientRect.bottom - 52, clientRect.right - 20, clientRect.bottom - 14);
    deviceContext.FillSolidRect(controlsRect, RGB(27, 29, 34));
    drawText(deviceContext, _T("|<    <    Play / Pause    >    >|"), controlsRect,
             EditorUi::kSecondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
