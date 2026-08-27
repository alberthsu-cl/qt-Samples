#pragma once

#include "ClipPropertiesStateResolver.h"
#include "MfcEditorPaneBase.h"

class MfcPropertiesPane final : public MfcEditorPaneBase
{
public:
    bool Create(CWnd *parent, UINT controlId);
    void setViewState(const ClipPropertiesViewState &viewState);

protected:
    CString paneTitle() const override;
    void drawContent(CDC &deviceContext, const CRect &clientRect) const override;

private:
    ClipPropertiesViewState viewState_;
};
