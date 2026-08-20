#include "MfcPropertiesPane.h"

#include "DemoProject.h"

#include <string>

bool MfcPropertiesPane::Create(CWnd *parent, UINT controlId)
{
    return createPane(parent, controlId);
}

CString MfcPropertiesPane::paneTitle() const
{
    return _T("Properties");
}

void MfcPropertiesPane::drawContent(CDC &deviceContext, const CRect &clientRect) const
{
    const auto &asset = demoAssets()[selectedAssetIndex()];
    const int left = 16;
    int top = EditorUi::kHeaderHeight + 18;

    const CString rows[] = {
        _T("Selected clip"),
        CString(_T("Name: ")) + asset.name,
        CString(_T("Type: ")) + asset.kind,
        CString(_T("Duration: ")) + asset.duration,
        CString(_T("Opacity: "))
            + std::to_wstring(clipSettings().opacityPercent).c_str() + _T("%"),
        CString(_T("Position: ")) + clipPositionDisplayName(clipSettings().position),
        CString(_T("Scale: "))
            + std::to_wstring(clipSettings().scalePercent).c_str() + _T("%")
    };

    for (int index = 0; index < _countof(rows); ++index) {
        const CRect rowRect(left, top, clientRect.right - left, top + 31);
        if (index == 0) {
            drawText(deviceContext, rows[index], rowRect, EditorUi::kText);
        } else {
            deviceContext.FillSolidRect(rowRect, RGB(45, 48, 56));
            drawText(deviceContext, rows[index], rowRect, EditorUi::kSecondaryText);
        }
        top += 39;
    }
}
