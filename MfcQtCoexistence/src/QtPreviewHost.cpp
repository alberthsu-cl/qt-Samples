#include "QtPreviewHost.h"

#include "QtEffectPreviewPanel.h"

#include <QImage>

QtPreviewHost::QtPreviewHost() = default;
QtPreviewHost::~QtPreviewHost() = default;

bool QtPreviewHost::create(void *mfcParentWindowHandle)
{
    previewPanel_ = std::make_unique<QtEffectPreviewPanel>();

    // This panel is drawn inside the MFC frame; it must never look like a
    // second top-level Qt window.
    previewPanel_->setWindowFlag(Qt::FramelessWindowHint, true);
    previewPanel_->setAttribute(Qt::WA_NativeWindow);

    const HWND qtWindowHandle = reinterpret_cast<HWND>(previewPanel_->winId());
    const HWND mfcWindowHandle = static_cast<HWND>(mfcParentWindowHandle);
    if (qtWindowHandle == nullptr || mfcWindowHandle == nullptr)
        return false;

    // Qt's QWidget now has an HWND. Reparent it into the existing MFC frame,
    // making this panel a native child region of the MFC application.
    ::SetParent(qtWindowHandle, mfcWindowHandle);
    const LONG_PTR style = ::GetWindowLongPtr(qtWindowHandle, GWL_STYLE);
    const LONG_PTR frameStyles = WS_POPUP | WS_CAPTION | WS_THICKFRAME |
                                WS_BORDER | WS_DLGFRAME;
    ::SetWindowLongPtr(qtWindowHandle, GWL_STYLE,
                       (style & ~frameStyles) | WS_CHILD | WS_CLIPSIBLINGS);
    ::SetWindowPos(qtWindowHandle, nullptr, 0, 0, 1, 1,
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    previewPanel_->show();
    return true;
}

void QtPreviewHost::resize(const CRect &bounds)
{
    if (!previewPanel_)
        return;

    // The MFC frame gives us physical child-window coordinates. Apply them to
    // the native HWND directly, rather than passing them through Qt's logical
    // high-DPI geometry conversion after the HWND was reparented to MFC.
    const HWND qtWindowHandle = reinterpret_cast<HWND>(previewPanel_->winId());
    ::SetWindowPos(qtWindowHandle, nullptr,
                   bounds.left, bounds.top, bounds.Width(), bounds.Height(),
                   SWP_NOZORDER | SWP_NOACTIVATE);
}

void QtPreviewHost::setImages(const CImage &originalImage,
                              const CImage &processedImage)
{
    if (!previewPanel_)
        return;

    previewPanel_->setImages(convertToQImage(originalImage),
                             convertToQImage(processedImage));
}

void QtPreviewHost::setShowingProcessedImage(bool showingProcessedImage)
{
    if (previewPanel_)
        previewPanel_->setShowingProcessedImage(showingProcessedImage);
}

QImage QtPreviewHost::convertToQImage(const CImage &image)
{
    if (image.IsNull())
        return {};

    // This intentionally simple bridge copies pixels from MFC's CImage into a
    // Qt-owned QImage. It keeps both frameworks independent during Phase 2.
    QImage converted(image.GetWidth(), image.GetHeight(), QImage::Format_RGBA8888);
    for (int y = 0; y < image.GetHeight(); ++y) {
        for (int x = 0; x < image.GetWidth(); ++x) {
            const COLORREF color = image.GetPixel(x, y);
            converted.setPixelColor(x, y,
                                    QColor(GetRValue(color),
                                           GetGValue(color),
                                           GetBValue(color)));
        }
    }

    return converted;
}
