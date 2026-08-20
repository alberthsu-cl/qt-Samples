#pragma once

#include "MfcEditorPaneBase.h"

class MfcTimelinePane final : public MfcEditorPaneBase
{
public:
    bool Create(CWnd *parent, UINT controlId);

protected:
    CString paneTitle() const override;
    void drawContent(CDC &deviceContext, const CRect &clientRect) const override;
};
