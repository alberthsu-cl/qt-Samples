#pragma once

#include "DemoProject.h"

#include <afxwin.h>

enum class EditorPaneKind {
    MediaLibrary,
    Preview,
    Properties,
    Timeline
};

// A deliberately simple custom MFC child window. Phase 0 uses one for every
// work area so we have a visual baseline before replacing individual panes.
class EditorPane final : public CWnd
{
public:
    bool Create(EditorPaneKind kind, CWnd *parent, UINT controlId);
    void setSelectedAssetIndex(int selectedAssetIndex);

protected:
    afx_msg void OnPaint();
    afx_msg void OnLButtonDown(UINT flags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    void drawMediaLibrary(CDC &deviceContext, const CRect &clientRect) const;
    void drawPreview(CDC &deviceContext, const CRect &clientRect) const;
    void drawProperties(CDC &deviceContext, const CRect &clientRect) const;
    void drawTimeline(CDC &deviceContext, const CRect &clientRect) const;
    void drawPaneTitle(CDC &deviceContext, const CString &title) const;
    void drawText(CDC &deviceContext, const CString &text, const CRect &bounds,
                  COLORREF color, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) const;
    int mediaAssetAt(CPoint point) const;

    EditorPaneKind kind_ = EditorPaneKind::MediaLibrary;
    int selectedAssetIndex_ = 0;
};
