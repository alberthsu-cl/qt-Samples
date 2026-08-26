#include "QtTimelineToolbarHost.h"

#include "QtTimelineToolbar.h"

#include <QObject>

#include <utility>

QtTimelineToolbarHost::QtTimelineToolbarHost() = default;
QtTimelineToolbarHost::~QtTimelineToolbarHost() = default;

bool QtTimelineToolbarHost::create(void *mfcParentWindowHandle)
{
    toolbar_ = std::make_unique<QtTimelineToolbar>();
    toolbar_->setWindowFlag(Qt::FramelessWindowHint, true);
    toolbar_->setAttribute(Qt::WA_NativeWindow);

    const HWND qtWindowHandle = reinterpret_cast<HWND>(toolbar_->winId());
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

    QObject::connect(toolbar_.get(), &QtTimelineToolbar::viewStateEdited,
                     toolbar_.get(), [this](int zoomPercent, bool isAudioTrackVisible,
                                            bool isRippleEditingEnabled) {
                         if (viewStateEditedHandler_)
                             viewStateEditedHandler_({ zoomPercent, isAudioTrackVisible,
                                                       isRippleEditingEnabled });
                     });
    QObject::connect(toolbar_.get(), &QtTimelineToolbar::fitTimelineRequested,
                     toolbar_.get(), [this] {
                         if (fitTimelineHandler_)
                             fitTimelineHandler_();
                     });
    toolbar_->show();
    return true;
}

void QtTimelineToolbarHost::resize(const CRect &bounds)
{
    if (!toolbar_)
        return;

    ::SetWindowPos(reinterpret_cast<HWND>(toolbar_->winId()), nullptr,
                   bounds.left, bounds.top, bounds.Width(), bounds.Height(),
                   SWP_NOZORDER | SWP_NOACTIVATE);
}

void QtTimelineToolbarHost::setViewState(const TimelineViewState &state)
{
    if (toolbar_)
        toolbar_->setViewState(state);
}

void QtTimelineToolbarHost::setViewStateEditedHandler(ViewStateEditedHandler handler)
{
    viewStateEditedHandler_ = std::move(handler);
}

void QtTimelineToolbarHost::setFitTimelineHandler(FitTimelineHandler handler)
{
    fitTimelineHandler_ = std::move(handler);
}
