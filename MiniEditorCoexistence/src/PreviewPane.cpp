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
    const CRect availableRect(20, EditorUi::kHeaderHeight + 18,
                              clientRect.right - 20, clientRect.bottom - 66);
    const int availableWidth = availableRect.Width();
    const int availableHeight = availableRect.Height();
    const int videoHeight = std::min(availableHeight, availableWidth * 9 / 16);
    const int videoWidth = videoHeight * 16 / 9;
    const int videoLeft = availableRect.left + (availableWidth - videoWidth) / 2;
    const int videoTop = availableRect.top + (availableHeight - videoHeight) / 2;
    const CRect videoRect(videoLeft, videoTop, videoLeft + videoWidth, videoTop + videoHeight);

    deviceContext.FillSolidRect(availableRect, EditorUi::kCanvasBackground);
    deviceContext.FillSolidRect(videoRect, asset.thumbnailColor);
    deviceContext.Draw3dRect(videoRect, RGB(220, 220, 220), RGB(220, 220, 220));

    CRect titleRect(videoRect.left + 12, videoRect.bottom - 38,
                    videoRect.right - 12, videoRect.bottom - 12);
    drawText(deviceContext, asset.name, titleRect, RGB(255, 255, 255),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    CRect controlsRect(20, clientRect.bottom - 52, clientRect.right - 20, clientRect.bottom - 14);
    deviceContext.FillSolidRect(controlsRect, RGB(27, 29, 34));
    drawText(deviceContext, _T("|<    <    Play / Pause    >    >|"), controlsRect,
             EditorUi::kSecondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
