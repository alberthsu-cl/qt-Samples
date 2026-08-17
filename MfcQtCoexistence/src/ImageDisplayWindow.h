#pragma once

#include <afxwin.h>
#include <atlimage.h>

// A small custom MFC child window that paints either the original image or the
// processed result, fitting it inside its rectangle while preserving aspect
// ratio. It does not own the two CImage objects.
class ImageDisplayWindow final : public CWnd
{
public:
    BOOL Create(CWnd *parent, const CRect &initialBounds);

    void setImages(const CImage *originalImage, const CImage *processedImage);
    void setShowProcessed(bool showProcessed);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC *deviceContext);

    DECLARE_MESSAGE_MAP()

private:
    const CImage *imageToDisplay() const;

    const CImage *originalImage_ = nullptr;
    const CImage *processedImage_ = nullptr;
    bool showProcessed_ = true;
};
