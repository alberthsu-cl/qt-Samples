#pragma once

#include "MfcEditorPaneBase.h"

// The MFC preview-rendering placeholder. It deliberately remains MFC in
// Phase 3, representing the existing native/GPU preview surface in a product.
class MfcPreviewCanvas final : public MfcEditorPaneBase
{
public:
    bool Create(CWnd *parent, UINT controlId);
    void setPreviewState(const PreviewState &state);

protected:
    CString paneTitle() const override;
    void drawContent(CDC &deviceContext, const CRect &clientRect) const override;

private:
    PreviewState previewState_;
};
