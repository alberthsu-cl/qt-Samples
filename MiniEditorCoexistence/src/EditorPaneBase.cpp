#include "EditorPaneBase.h"

#include <algorithm>

BEGIN_MESSAGE_MAP(EditorPaneBase, CWnd)
    ON_WM_PAINT()
END_MESSAGE_MAP()

bool EditorPaneBase::createPane(CWnd *parent, UINT controlId)
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

void EditorPaneBase::setSelectedAssetIndex(int selectedAssetIndex)
{
    selectedAssetIndex_ = std::clamp(selectedAssetIndex, 0, 5);
    Invalidate();
}

void EditorPaneBase::setClipSettings(const ClipSettings &settings)
{
    clipSettings_ = settings;
    Invalidate();
}

void EditorPaneBase::setPlaybackState(const PlaybackState &state)
{
    playbackState_ = state;
    Invalidate();
}

int EditorPaneBase::selectedAssetIndex() const
{
    return selectedAssetIndex_;
}

const ClipSettings &EditorPaneBase::clipSettings() const
{
    return clipSettings_;
}

const PlaybackState &EditorPaneBase::playbackState() const
{
    return playbackState_;
}

void EditorPaneBase::OnPaint()
{
    CPaintDC deviceContext(this);
    CRect clientRect;
    GetClientRect(&clientRect);

    deviceContext.FillSolidRect(clientRect, EditorUi::kPanelBackground);
    deviceContext.Draw3dRect(clientRect, EditorUi::kPanelBorder, EditorUi::kPanelBorder);
    drawPaneTitle(deviceContext, paneTitle());
    drawContent(deviceContext, clientRect);
}

void EditorPaneBase::drawPaneTitle(CDC &deviceContext, const CString &title) const
{
    CRect clientRect;
    GetClientRect(&clientRect);
    const CRect titleRect(0, 0, clientRect.right, EditorUi::kHeaderHeight);
    deviceContext.FillSolidRect(titleRect, RGB(47, 50, 58));
    drawText(deviceContext, title,
             CRect(12, 0, clientRect.right - 12, EditorUi::kHeaderHeight),
             EditorUi::kText);
}

void EditorPaneBase::drawText(CDC &deviceContext, const CString &text,
                              const CRect &bounds, COLORREF color, UINT format) const
{
    const int previousBackgroundMode = deviceContext.SetBkMode(TRANSPARENT);
    const COLORREF previousColor = deviceContext.SetTextColor(color);
    deviceContext.DrawText(text, const_cast<CRect *>(&bounds), format);
    deviceContext.SetTextColor(previousColor);
    deviceContext.SetBkMode(previousBackgroundMode);
}
