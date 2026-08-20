#include "MfcMediaLibraryPane.h"

#include "DemoProject.h"
#include "resource.h"

namespace {

constexpr int kAssetWidth = 132;
constexpr int kAssetHeight = 112;
constexpr int kAssetSpacing = 12;

} // namespace

BEGIN_MESSAGE_MAP(MfcMediaLibraryPane, MfcEditorPaneBase)
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

bool MfcMediaLibraryPane::Create(CWnd *parent, UINT controlId)
{
    return createPane(parent, controlId);
}

CString MfcMediaLibraryPane::paneTitle() const
{
    return _T("Media Library (MFC baseline)");
}

void MfcMediaLibraryPane::drawContent(CDC &deviceContext, const CRect &clientRect) const
{
    CRect categoryRect(10, EditorUi::kHeaderHeight + 8,
                       clientRect.Width() - 10, EditorUi::kHeaderHeight + 38);
    deviceContext.FillSolidRect(categoryRect, RGB(49, 53, 62));
    drawText(deviceContext, _T("My Media    Stock Media    Backgrounds"), categoryRect,
             EditorUi::kText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const auto &assets = demoAssets();
    for (int index = 0; index < static_cast<int>(assets.size()); ++index) {
        const int column = index % 2;
        const int row = index / 2;
        const int x = 12 + column * (kAssetWidth + kAssetSpacing);
        const int y = EditorUi::kHeaderHeight + 54 + row * (kAssetHeight + kAssetSpacing);
        const CRect assetRect(x, y, x + kAssetWidth, y + kAssetHeight);

        if (index == selectedAssetIndex())
            deviceContext.Draw3dRect(assetRect, EditorUi::kAccent, EditorUi::kAccent);

        const CRect thumbnailRect(assetRect.left + 4, assetRect.top + 4,
                                  assetRect.right - 4, assetRect.top + 70);
        deviceContext.FillSolidRect(thumbnailRect, assets[index].thumbnailColor);

        CRect typeRect(thumbnailRect.left + 5, thumbnailRect.top + 5,
                       thumbnailRect.left + 55, thumbnailRect.top + 24);
        deviceContext.FillSolidRect(typeRect, RGB(25, 27, 32));
        drawText(deviceContext, assets[index].kind, typeRect, EditorUi::kText,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        CRect nameRect(assetRect.left + 4, assetRect.top + 75,
                       assetRect.right - 4, assetRect.top + 95);
        drawText(deviceContext, assets[index].name, nameRect, EditorUi::kText,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        CRect durationRect(assetRect.left + 4, assetRect.top + 95,
                           assetRect.right - 4, assetRect.bottom - 4);
        drawText(deviceContext, assets[index].duration, durationRect, EditorUi::kSecondaryText,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void MfcMediaLibraryPane::OnLButtonDown(UINT flags, CPoint point)
{
    const int assetIndex = mediaAssetAt(point);
    if (assetIndex >= 0) {
        GetParent()->SendMessage(WM_COMMAND,
                                 MAKEWPARAM(ID_MEDIA_ASSET_FIRST + assetIndex, 0));
    }

    MfcEditorPaneBase::OnLButtonDown(flags, point);
}

int MfcMediaLibraryPane::mediaAssetAt(CPoint point) const
{
    if (point.y < EditorUi::kHeaderHeight + 54)
        return -1;

    const int column = (point.x - 12) / (kAssetWidth + kAssetSpacing);
    const int row = (point.y - (EditorUi::kHeaderHeight + 54))
                    / (kAssetHeight + kAssetSpacing);
    if (column < 0 || column > 1 || row < 0)
        return -1;

    const int assetIndex = row * 2 + column;
    if (assetIndex >= static_cast<int>(demoAssets().size()))
        return -1;

    const int x = 12 + column * (kAssetWidth + kAssetSpacing);
    const int y = EditorUi::kHeaderHeight + 54 + row * (kAssetHeight + kAssetSpacing);
    const CRect assetRect(x, y, x + kAssetWidth, y + kAssetHeight);
    return assetRect.PtInRect(point) ? assetIndex : -1;
}
