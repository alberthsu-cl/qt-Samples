#include "QtPropertiesHost.h"

#include "QtPropertiesPanel.h"

#include <QObject>

#include <utility>

QtPropertiesHost::QtPropertiesHost() = default;
QtPropertiesHost::~QtPropertiesHost() = default;

bool QtPropertiesHost::create(void *mfcParentWindowHandle)
{
    panel_ = std::make_unique<QtPropertiesPanel>();
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

    QObject::connect(panel_.get(), &QtPropertiesPanel::clipSettingsEdited,
                     panel_.get(),
                     [this](int opacityPercent, int scalePercent, int positionValue) {
                         if (clipSettingsEditedHandler_) {
                             clipSettingsEditedHandler_(ClipSettings{
                                 opacityPercent,
                                 scalePercent,
                                 static_cast<ClipPosition>(positionValue)
                             });
                         }
                     });
    panel_->show();
    return true;
}

void QtPropertiesHost::resize(const CRect &bounds)
{
    if (!panel_)
        return;

    const HWND qtWindowHandle = reinterpret_cast<HWND>(panel_->winId());
    ::SetWindowPos(qtWindowHandle, nullptr,
                   bounds.left, bounds.top, bounds.Width(), bounds.Height(),
                   SWP_NOZORDER | SWP_NOACTIVATE);
}

void QtPropertiesHost::setSelectedAsset(const wchar_t *name, const wchar_t *kind,
                                        const ClipSettings &settings)
{
    if (panel_)
        panel_->setSelectedAsset(name, kind, settings);
}

void QtPropertiesHost::setEditingEnabled(bool enabled)
{
    if (panel_)
        panel_->setEditingEnabled(enabled);
}

void QtPropertiesHost::setClipSettingsEditedHandler(ClipSettingsEditedHandler handler)
{
    clipSettingsEditedHandler_ = std::move(handler);
}
