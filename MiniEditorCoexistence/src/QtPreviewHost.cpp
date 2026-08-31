#include "QtPreviewHost.h"

#include "QtPreviewPanel.h"

#include <QWidget>

QtPreviewHost::QtPreviewHost() = default;
QtPreviewHost::~QtPreviewHost() = default;

bool QtPreviewHost::create(void *mfcParentWindowHandle)
{
    panel_ = std::make_unique<QtPreviewPanel>();
    panel_->setWindowFlag(Qt::FramelessWindowHint, true);
    panel_->setAttribute(Qt::WA_NativeWindow);

    const HWND qtWindowHandle = reinterpret_cast<HWND>(panel_->winId());
    const HWND mfcWindowHandle = static_cast<HWND>(mfcParentWindowHandle);
    if (qtWindowHandle == nullptr || mfcWindowHandle == nullptr)
        return false;

    ::SetParent(qtWindowHandle, mfcWindowHandle);
    const LONG_PTR style = ::GetWindowLongPtr(qtWindowHandle, GWL_STYLE);
    const LONG_PTR frameStyles = WS_POPUP | WS_CAPTION | WS_THICKFRAME |
                                WS_BORDER | WS_DLGFRAME;
    ::SetWindowLongPtr(qtWindowHandle, GWL_STYLE,
                       (style & ~frameStyles) | WS_CHILD | WS_CLIPSIBLINGS);
    ::SetWindowPos(qtWindowHandle, nullptr, 0, 0, 1, 1,
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    panel_->show();
    return true;
}

void QtPreviewHost::resize(const CRect &bounds)
{
    if (!panel_)
        return;

    ::SetWindowPos(reinterpret_cast<HWND>(panel_->winId()), nullptr,
                   bounds.left, bounds.top, bounds.Width(), bounds.Height(),
                   SWP_NOZORDER | SWP_NOACTIVATE);
}

void QtPreviewHost::setPreviewState(const PreviewState &state)
{
    if (panel_)
        panel_->setPreviewState(state);
}

void QtPreviewHost::setPlaybackState(const PlaybackState &state)
{
    if (panel_)
        panel_->setPlaybackState(state);
}

void QtPreviewHost::setFallbackImage(const QImage &image)
{
    if (panel_)
        panel_->setFallbackImage(image);
}

QVideoSink *QtPreviewHost::videoSink() const
{
    return panel_ ? panel_->videoSink() : nullptr;
}

void QtPreviewHost::setDecodedVideoVisible(bool visible)
{
    if (panel_)
        panel_->setDecodedVideoVisible(visible);
}
