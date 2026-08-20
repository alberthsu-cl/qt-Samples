#pragma once

#include <afxwin.h>

#include <functional>

// A small MFC-owned splitter handle. It knows only how to capture the mouse
// and report a parent-client coordinate; MainFrame owns all layout policy.
class WorkspaceSplitter final : public CWnd
{
public:
    enum class Orientation {
        Vertical,
        Horizontal
    };

    using DragHandler = std::function<void(int parentCoordinate)>;

    bool Create(Orientation orientation, CWnd *parent, UINT controlId);
    void setDragHandler(DragHandler handler);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnSetCursor(CWnd *window, UINT hitTest, UINT mouseMessage);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnLButtonUp(UINT flags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    int parentCoordinate(CPoint localPoint) const;

    Orientation orientation_ = Orientation::Vertical;
    DragHandler dragHandler_;
    bool isDragging_ = false;
};
