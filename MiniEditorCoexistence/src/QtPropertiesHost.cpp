#include "QtPropertiesHost.h"

#include "QtPropertiesPanel.h"

#include <QObject>
#include <QSizePolicy>
#include <QTimer>

#include <utility>

QtPropertiesHost::QtPropertiesHost() = default;
QtPropertiesHost::~QtPropertiesHost() = default;

bool QtPropertiesHost::create(void *mfcParentWindowHandle)
{
    panel_ = std::make_unique<QtPropertiesPanel>();
    // MFC owns this native child's rectangle. When the panel switches from a
    // short read-only message to the full clip editor, Qt must not promote its
    // layout size hint into a larger top-level window that covers siblings.
    panel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    panel_->setMinimumSize(0, 0);
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
                     [this](int opacityPercent, int scalePercent, int positionValue,
                            int fadeInFrames, int fadeOutFrames,
                            int effectValue, int effectIntensityPercent) {
                         if (clipSettingsEditedHandler_) {
                             clipSettingsEditedHandler_(ClipSettings{
                                 opacityPercent,
                                 scalePercent,
                                 static_cast<ClipPosition>(positionValue),
                                 fadeInFrames,
                                 fadeOutFrames,
                                 static_cast<ClipEffectKind>(effectValue),
                                 effectIntensityPercent
                             });
                         }
                     });
    panel_->show();
    return true;
}

void QtPropertiesHost::resize(const CRect &bounds)
{
    bounds_ = bounds;
    hasBounds_ = true;
    applyBounds();
}

void QtPropertiesHost::setViewState(const ClipPropertiesViewState &viewState)
{
    if (!panel_)
        return;

    panel_->setViewState(viewState);
    applyBounds();
    // Visibility changes queue a Qt layout request. Apply the MFC rectangle
    // one more time after that request, otherwise the new form can temporarily
    // retain its preferred height and paint over the timeline ruler.
    QTimer::singleShot(0, panel_.get(), [this] { applyBounds(); });
}

void QtPropertiesHost::setClipSettingsEditedHandler(ClipSettingsEditedHandler handler)
{
    clipSettingsEditedHandler_ = std::move(handler);
}

void QtPropertiesHost::applyBounds()
{
    if (!panel_ || !hasBounds_)
        return;

    const HWND qtWindowHandle = reinterpret_cast<HWND>(panel_->winId());
    ::SetWindowPos(qtWindowHandle, nullptr,
                   bounds_.left, bounds_.top,
                   bounds_.Width(), bounds_.Height(),
                   SWP_NOZORDER | SWP_NOACTIVATE);
}
