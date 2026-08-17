#include "ImageToggleButton.h"

BOOL ImageToggleButton::Create(CWnd *parent,
                               const CRect &initialBounds,
                               UINT controlId)
{
    createButtonImages();

    // BS_OWNERDRAW asks MFC to call DrawItem(), where we paint only the image.
    return CButton::Create(_T("Show original"),
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                           initialBounds,
                           parent,
                           controlId);
}

void ImageToggleButton::setShowingProcessedImage(bool showingProcessedImage)
{
    showingProcessedImage_ = showingProcessedImage;

    // The text is not drawn, but it remains available to accessibility tools.
    SetWindowText(showingProcessedImage_ ? _T("Applied effect is enabled")
                                         : _T("Original image is enabled"));
    Invalidate();
}

void ImageToggleButton::DrawItem(LPDRAWITEMSTRUCT drawItem)
{
    CDC deviceContext;
    deviceContext.Attach(drawItem->hDC);

    const CRect buttonRect(drawItem->rcItem);
    const bool isPressed = (drawItem->itemState & ODS_SELECTED) != 0;
    const COLORREF background = isPressed ? RGB(185, 185, 185) : RGB(230, 230, 230);
    deviceContext.FillSolidRect(buttonRect, background);
    deviceContext.DrawEdge(const_cast<CRect *>(&buttonRect),
                           isPressed ? BDR_SUNKENOUTER : BDR_RAISEDOUTER,
                           BF_RECT);

    // The checked icon represents the current state, not the next action.
    // This makes the button behave like a visual on/off switch for the effect.
    const CImage &icon = showingProcessedImage_
                             ? appliedImageButton_
                             : originalImageButton_;
    const int iconWidth = icon.GetWidth();
    const int iconHeight = icon.GetHeight();
    const int left = buttonRect.left + (buttonRect.Width() - iconWidth) / 2;
    const int top = buttonRect.top + (buttonRect.Height() - iconHeight) / 2;
    icon.Draw(deviceContext.GetSafeHdc(), left, top, iconWidth, iconHeight);

    deviceContext.Detach();
}

void ImageToggleButton::createButtonImages()
{
    drawCheckBox(originalImageButton_, false);
    drawCheckBox(appliedImageButton_, true);
}

void ImageToggleButton::drawCheckBox(CImage &image, bool isChecked)
{
    constexpr int width = 28;
    constexpr int height = 28;

    image.Destroy();
    if (FAILED(image.Create(width, height, 32)))
        return;

    // Match the standard button background, then draw a checkbox in the center.
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            image.SetPixel(x, y, RGB(230, 230, 230));

    constexpr int left = 4;
    constexpr int top = 4;
    constexpr int right = 23;
    constexpr int bottom = 23;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const bool isBorder = x == left || x == right || y == top || y == bottom;
            image.SetPixel(x, y, isBorder ? RGB(70, 70, 70) : RGB(255, 255, 255));
        }
    }

    if (!isChecked)
        return;

    // Draw a thick green check mark: down-right, then up-right.
    for (int offset = 0; offset < 7; ++offset) {
        for (int thickness = -1; thickness <= 1; ++thickness) {
            image.SetPixel(8 + offset, 13 + offset + thickness, RGB(30, 150, 65));
            image.SetPixel(14 + offset, 19 - offset + thickness, RGB(30, 150, 65));
        }
    }
}
