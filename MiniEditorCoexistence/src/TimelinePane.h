#pragma once

#include "EditorPaneBase.h"

class TimelinePane final : public EditorPaneBase
{
public:
    bool Create(CWnd *parent, UINT controlId);

protected:
    CString paneTitle() const override;
    void drawContent(CDC &deviceContext, const CRect &clientRect) const override;
};
