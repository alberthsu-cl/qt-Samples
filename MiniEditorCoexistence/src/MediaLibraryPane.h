#pragma once

#include "EditorPaneBase.h"

class MediaLibraryPane final : public EditorPaneBase
{
public:
    bool Create(CWnd *parent, UINT controlId);

protected:
    CString paneTitle() const override;
    void drawContent(CDC &deviceContext, const CRect &clientRect) const override;
    afx_msg void OnLButtonDown(UINT flags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    int mediaAssetAt(CPoint point) const;
};
