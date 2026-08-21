#include "MfcEditorPaneBase.h"

#include "MfcDoubleBufferedPaint.h"

#include <algorithm>

BEGIN_MESSAGE_MAP(MfcEditorPaneBase, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

bool MfcEditorPaneBase::createPane(CWnd *parent, UINT controlId)
{
    const CString windowClass = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(::GetStockObject(NULL_BRUSH)),
        nullptr);

    return CWnd::CreateEx(0, windowClass, _T(""),
                           WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                           CRect(0, 0, 1, 1), parent, controlId) != FALSE;
}

void MfcEditorPaneBase::setSelectedAssetIndex(int selectedAssetIndex)
{
    selectedAssetIndex_ = std::clamp(selectedAssetIndex, 0, 5);
    Invalidate(FALSE);
}

void MfcEditorPaneBase::setClipSettings(const ClipSettings &settings)
{
    clipSettings_ = settings;
    Invalidate(FALSE);
}

void MfcEditorPaneBase::setPlaybackState(const PlaybackState &state)
{
    playbackState_ = state;
    Invalidate(FALSE);
}

int MfcEditorPaneBase::selectedAssetIndex() const
{
    return selectedAssetIndex_;
}

const ClipSettings &MfcEditorPaneBase::clipSettings() const
{
    return clipSettings_;
}

const PlaybackState &MfcEditorPaneBase::playbackState() const
{
    return playbackState_;
}

void MfcEditorPaneBase::OnPaint()
{
    MfcDoubleBufferedPaint paint(this);
    CDC &deviceContext = paint.deviceContext();
    CRect clientRect;
    GetClientRect(&clientRect);

    deviceContext.FillSolidRect(clientRect, EditorUi::kPanelBackground);
    deviceContext.Draw3dRect(clientRect, EditorUi::kPanelBorder, EditorUi::kPanelBorder);
    drawPaneTitle(deviceContext, paneTitle());
    drawContent(deviceContext, clientRect);
}

BOOL MfcEditorPaneBase::OnEraseBkgnd(CDC *)
{
    // OnPaint always supplies the full background through the back buffer.
    // Suppress the separate erase pass that causes visible flashing.
    return TRUE;
}

void MfcEditorPaneBase::drawPaneTitle(CDC &deviceContext, const CString &title) const
{
    CRect clientRect;
    GetClientRect(&clientRect);
    const CRect titleRect(0, 0, clientRect.right, EditorUi::kHeaderHeight);
    deviceContext.FillSolidRect(titleRect, RGB(47, 50, 58));
    drawText(deviceContext, title,
             CRect(12, 0, clientRect.right - 12, EditorUi::kHeaderHeight),
             EditorUi::kText);
}

void MfcEditorPaneBase::drawText(CDC &deviceContext, const CString &text,
                              const CRect &bounds, COLORREF color, UINT format) const
{
    const int previousBackgroundMode = deviceContext.SetBkMode(TRANSPARENT);
    const COLORREF previousColor = deviceContext.SetTextColor(color);
    deviceContext.DrawText(text, const_cast<CRect *>(&bounds), format);
    deviceContext.SetTextColor(previousColor);
    deviceContext.SetBkMode(previousBackgroundMode);
}
