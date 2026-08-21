#pragma once

#include <afxwin.h>

// Paint into an off-screen bitmap, then copy the complete image to the MFC
// window once. This avoids showing a partly drawn frame during frequent UI
// updates such as playback.
class MfcDoubleBufferedPaint final
{
public:
    explicit MfcDoubleBufferedPaint(CWnd *window);
    ~MfcDoubleBufferedPaint();

    MfcDoubleBufferedPaint(const MfcDoubleBufferedPaint &) = delete;
    MfcDoubleBufferedPaint &operator=(const MfcDoubleBufferedPaint &) = delete;

    CDC &deviceContext();

private:
    CPaintDC paintDeviceContext_;
    CDC backBufferDeviceContext_;
    CBitmap backBufferBitmap_;
    CBitmap *previousBitmap_ = nullptr;
    CRect clientRect_;
    CDC *drawingDeviceContext_ = nullptr;
    bool usesBackBuffer_ = false;
};
