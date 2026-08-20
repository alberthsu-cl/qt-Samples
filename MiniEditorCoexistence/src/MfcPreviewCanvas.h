#pragma once

#include "EditorPaneBase.h"

// The MFC preview-rendering placeholder. It deliberately remains MFC in
// Phase 3, representing the existing native/GPU preview surface in a product.
class MfcPreviewCanvas final : public EditorPaneBase
{
public:
    bool Create(CWnd *parent, UINT controlId);

protected:
    CString paneTitle() const override;
    void drawContent(CDC &deviceContext, const CRect &clientRect) const override;
};
