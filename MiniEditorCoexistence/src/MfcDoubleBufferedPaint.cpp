#include "MfcDoubleBufferedPaint.h"

MfcDoubleBufferedPaint::MfcDoubleBufferedPaint(CWnd *window)
    : paintDeviceContext_(window)
{
    window->GetClientRect(&clientRect_);
    drawingDeviceContext_ = &paintDeviceContext_;

    if (clientRect_.Width() <= 0 || clientRect_.Height() <= 0)
        return;

    if (!backBufferDeviceContext_.CreateCompatibleDC(&paintDeviceContext_))
        return;
    if (!backBufferBitmap_.CreateCompatibleBitmap(&paintDeviceContext_,
                                                  clientRect_.Width(), clientRect_.Height())) {
        return;
    }

    previousBitmap_ = backBufferDeviceContext_.SelectObject(&backBufferBitmap_);
    drawingDeviceContext_ = &backBufferDeviceContext_;
    usesBackBuffer_ = true;
}

MfcDoubleBufferedPaint::~MfcDoubleBufferedPaint()
{
    if (!usesBackBuffer_)
        return;

    paintDeviceContext_.BitBlt(0, 0, clientRect_.Width(), clientRect_.Height(),
                               &backBufferDeviceContext_, 0, 0, SRCCOPY);
    backBufferDeviceContext_.SelectObject(previousBitmap_);
}

CDC &MfcDoubleBufferedPaint::deviceContext()
{
    return *drawingDeviceContext_;
}
