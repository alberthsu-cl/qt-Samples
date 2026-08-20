#include "WorkspaceSplitter.h"

#include <utility>

BEGIN_MESSAGE_MAP(WorkspaceSplitter, CWnd)
    ON_WM_PAINT()
    ON_WM_SETCURSOR()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

bool WorkspaceSplitter::Create(Orientation orientation, CWnd *parent, UINT controlId)
{
    orientation_ = orientation;
    const CString windowClass = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(::GetStockObject(NULL_BRUSH)), nullptr);
    return CWnd::CreateEx(0, windowClass, _T(""),
                           WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                           CRect(0, 0, 1, 1), parent, controlId) != FALSE;
}

void WorkspaceSplitter::setDragHandler(DragHandler handler)
{
    dragHandler_ = std::move(handler);
}

void WorkspaceSplitter::OnPaint()
{
    CPaintDC deviceContext(this);
    CRect clientRect;
    GetClientRect(&clientRect);
    deviceContext.FillSolidRect(clientRect, RGB(43, 46, 54));

    const COLORREF handleColor = isDragging_ ? RGB(63, 143, 225) : RGB(80, 85, 96);
    if (orientation_ == Orientation::Vertical) {
        const int x = clientRect.Width() / 2;
        deviceContext.FillSolidRect(x, 0, 1, clientRect.Height(), handleColor);
    } else {
        const int y = clientRect.Height() / 2;
        deviceContext.FillSolidRect(0, y, clientRect.Width(), 1, handleColor);
    }
}

BOOL WorkspaceSplitter::OnSetCursor(CWnd *, UINT, UINT)
{
    const LPCTSTR cursorId = orientation_ == Orientation::Vertical ? IDC_SIZEWE : IDC_SIZENS;
    ::SetCursor(::LoadCursor(nullptr, cursorId));
    return TRUE;
}

void WorkspaceSplitter::OnLButtonDown(UINT flags, CPoint point)
{
    isDragging_ = true;
    SetCapture();
    Invalidate();
    CWnd::OnLButtonDown(flags, point);
}

void WorkspaceSplitter::OnMouseMove(UINT flags, CPoint point)
{
    if (isDragging_ && GetCapture() == this && dragHandler_)
        dragHandler_(parentCoordinate(point));

    CWnd::OnMouseMove(flags, point);
}

void WorkspaceSplitter::OnLButtonUp(UINT flags, CPoint point)
{
    if (isDragging_) {
        if (dragHandler_)
            dragHandler_(parentCoordinate(point));
        ReleaseCapture();
        isDragging_ = false;
        Invalidate();
    }

    CWnd::OnLButtonUp(flags, point);
}

int WorkspaceSplitter::parentCoordinate(CPoint localPoint) const
{
    CPoint screenPoint = localPoint;
    ClientToScreen(&screenPoint);
    CWnd *parent = GetParent();
    parent->ScreenToClient(&screenPoint);
    return orientation_ == Orientation::Vertical ? screenPoint.x : screenPoint.y;
}
