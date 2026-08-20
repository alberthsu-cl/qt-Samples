#include "QtTransportHost.h"

#include "QtTransportPanel.h"

#include <QObject>

#include <utility>

QtTransportHost::QtTransportHost() = default;
QtTransportHost::~QtTransportHost() = default;

bool QtTransportHost::create(void *mfcParentWindowHandle)
{
    panel_ = std::make_unique<QtTransportPanel>();
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

    QObject::connect(panel_.get(), &QtTransportPanel::playbackCommandRequested,
                     panel_.get(),
                     [this](int commandValue) {
                         if (playbackCommandHandler_) {
                             playbackCommandHandler_(
                                 static_cast<PlaybackCommand>(commandValue));
                         }
                     });
    panel_->show();
    return true;
}

void QtTransportHost::resize(const CRect &bounds)
{
    if (!panel_)
        return;

    const HWND qtWindowHandle = reinterpret_cast<HWND>(panel_->winId());
    ::SetWindowPos(qtWindowHandle, nullptr,
                   bounds.left, bounds.top, bounds.Width(), bounds.Height(),
                   SWP_NOZORDER | SWP_NOACTIVATE);
}

void QtTransportHost::setPlaybackState(const PlaybackState &state)
{
    if (panel_)
        panel_->setPlaybackState(state);
}

void QtTransportHost::setPlaybackCommandHandler(PlaybackCommandHandler handler)
{
    playbackCommandHandler_ = std::move(handler);
}
