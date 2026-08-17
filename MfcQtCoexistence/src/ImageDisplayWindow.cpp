#include "ImageDisplayWindow.h"

#include <algorithm>

BOOL ImageDisplayWindow::Create(CWnd *parent, const CRect &initialBounds)
{
    const CString className = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(nullptr, IDC_ARROW),
        static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)));

    return CWnd::CreateEx(0,
                          className,
                          _T("Image Display"),
                          WS_CHILD | WS_VISIBLE,
                          initialBounds,
                          parent,
                          0);
}

void ImageDisplayWindow::setImages(const CImage *originalImage,
                                   const CImage *processedImage)
{
    originalImage_ = originalImage;
    processedImage_ = processedImage;
    Invalidate();
}

void ImageDisplayWindow::setShowProcessed(bool showProcessed)
{
    showProcessed_ = showProcessed;
    Invalidate();
}

void ImageDisplayWindow::OnPaint()
{
    CPaintDC deviceContext(this);
    CRect clientRect;
    GetClientRect(&clientRect);
    deviceContext.FillSolidRect(clientRect, RGB(20, 20, 20));

    const CImage *image = imageToDisplay();
    if (image == nullptr || image->IsNull()) {
        deviceContext.SetBkMode(TRANSPARENT);
        deviceContext.SetTextColor(RGB(220, 220, 220));
        deviceContext.DrawText(_T("No image loaded."), clientRect,
                               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    const double scaleX = static_cast<double>(clientRect.Width()) / image->GetWidth();
    const double scaleY = static_cast<double>(clientRect.Height()) / image->GetHeight();
    const double scale = std::min(scaleX, scaleY);
    const int drawnWidth = static_cast<int>(image->GetWidth() * scale);
    const int drawnHeight = static_cast<int>(image->GetHeight() * scale);
    const int left = clientRect.left + (clientRect.Width() - drawnWidth) / 2;
    const int top = clientRect.top + (clientRect.Height() - drawnHeight) / 2;

    image->Draw(deviceContext.GetSafeHdc(), left, top, drawnWidth, drawnHeight);
}

BOOL ImageDisplayWindow::OnEraseBkgnd(CDC *)
{
    // OnPaint fills the entire client area, so suppress the extra erase pass.
    return TRUE;
}

const CImage *ImageDisplayWindow::imageToDisplay() const
{
    return showProcessed_ ? processedImage_ : originalImage_;
}

BEGIN_MESSAGE_MAP(ImageDisplayWindow, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()
