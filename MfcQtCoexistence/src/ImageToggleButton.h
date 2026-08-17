#pragma once

#include <afxwin.h>
#include <atlimage.h>

// An owner-drawn button with two real CImage objects instead of visible text.
// The icon indicates the current display state:
//   - unchecked: original image is displayed
//   - checked: applied effect is displayed
class ImageToggleButton final : public CButton
{
public:
    BOOL Create(CWnd *parent, const CRect &initialBounds, UINT controlId);

    void setShowingProcessedImage(bool showingProcessedImage);

protected:
    void DrawItem(LPDRAWITEMSTRUCT drawItem) override;

private:
    void createButtonImages();
    void drawCheckBox(CImage &image, bool isChecked);

    CImage originalImageButton_;
    CImage appliedImageButton_;
    bool showingProcessedImage_ = true;
};
