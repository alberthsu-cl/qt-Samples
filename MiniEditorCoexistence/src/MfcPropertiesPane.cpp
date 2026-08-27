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
    const CString editState = viewState_.editingEnabled
        ? _T("Timeline clip") : _T("Select a timeline clip to edit");

    if (viewState_.target != ClipPropertiesTarget::TimelineClip) {
        const CString message = viewState_.target == ClipPropertiesTarget::MediaAsset
            ? _T("Media asset selected. Add it to the timeline to edit clip properties.")
            : _T("Select a timeline clip to edit its properties.");
        drawText(deviceContext, message,
                 CRect(left, top, clientRect.right - left, top + 64),
                 EditorUi::kSecondaryText, DT_LEFT | DT_TOP | DT_WORDBREAK);
        return;
    }

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
