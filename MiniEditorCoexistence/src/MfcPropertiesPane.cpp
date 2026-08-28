#include "MfcPropertiesPane.h"

#include <string>
#include <vector>

bool MfcPropertiesPane::Create(CWnd *parent, UINT controlId)
{
    return createPane(parent, controlId);
}

void MfcPropertiesPane::setViewState(const ClipPropertiesViewState &viewState)
{
    viewState_ = viewState;
    Invalidate(FALSE);
}

CString MfcPropertiesPane::paneTitle() const
{
    return _T("Properties");
}

void MfcPropertiesPane::drawContent(CDC &deviceContext, const CRect &clientRect) const
{
    const int left = 16;
    int top = EditorUi::kHeaderHeight + 18;

    const CString mediaKind = viewState_.mediaKind == MediaKind::Audio
        ? _T("Audio") : (viewState_.mediaKind == MediaKind::Image
            ? _T("Image") : _T("Video"));
    if (viewState_.target == ClipPropertiesTarget::EmptyTimeline) {
        const CString message = _T("Select a timeline clip to edit its properties.");
        drawText(deviceContext, message,
                 CRect(left, top, clientRect.right - left, top + 64),
                 EditorUi::kSecondaryText, DT_LEFT | DT_TOP | DT_WORDBREAK);
        return;
    }

    if (viewState_.target == ClipPropertiesTarget::MediaAsset) {
        const CString displayName = viewState_.mediaDisplayName.empty()
            ? _T("Unknown media") : viewState_.mediaDisplayName.c_str();
        const CString filePath = viewState_.mediaFilePath.empty()
            ? _T("Unavailable") : viewState_.mediaFilePath.c_str();
        const std::vector<CString> rows{
            CString(_T("Name: ")) + displayName,
            CString(_T("Type: ")) + mediaKind,
            CString(_T("Duration: "))
                + std::to_wstring(viewState_.durationFrames).c_str() + _T(" f"),
            CString(_T("Source: ")) + filePath,
            _T("Add this media to the timeline to edit placement properties.")
        };
        for (const CString &row : rows) {
            const CRect rowRect(left, top, clientRect.right - left, top + 31);
            deviceContext.FillSolidRect(rowRect, RGB(45, 48, 56));
            drawText(deviceContext, row, rowRect, EditorUi::kSecondaryText,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            top += 39;
        }
        return;
    }

    const CString editState = _T("Timeline clip");
    std::vector<CString> rows{
        editState,
        CString(_T("Type: ")) + mediaKind,
        CString(_T("Duration: "))
            + std::to_wstring(viewState_.durationFrames).c_str() + _T(" f")
    };
    if (viewState_.mediaKind != MediaKind::Audio) {
        rows.push_back(CString(_T("Opacity: "))
            + std::to_wstring(viewState_.settings.opacityPercent).c_str() + _T("%"));
        rows.push_back(CString(_T("Position: "))
            + clipPositionDisplayName(viewState_.settings.position));
        rows.push_back(CString(_T("Scale: "))
            + std::to_wstring(viewState_.settings.scalePercent).c_str() + _T("%"));
    }
    rows.push_back(CString(viewState_.mediaKind == MediaKind::Audio
                              ? _T("Level fade: in ") : _T("Fade: in "))
        + std::to_wstring(viewState_.settings.fadeInFrames).c_str() + _T("f, out ")
        + std::to_wstring(viewState_.settings.fadeOutFrames).c_str() + _T("f"));

    for (std::size_t index = 0; index < rows.size(); ++index) {
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
