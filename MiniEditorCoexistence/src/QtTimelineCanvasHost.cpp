#include "QtTimelineCanvasHost.h"

#include "QtTimelineCanvas.h"

#include <QObject>
#include <QScrollArea>

#include <utility>

QtTimelineCanvasHost::QtTimelineCanvasHost() = default;
QtTimelineCanvasHost::~QtTimelineCanvasHost() = default;

bool QtTimelineCanvasHost::create(void *mfcParentWindowHandle)
{
    scrollArea_ = std::make_unique<QScrollArea>();
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Let the canvas fill the available track height. Its minimum width still
    // controls horizontal overflow, which is what produces the timeline bar.
    scrollArea_->setWidgetResizable(true);
    canvas_ = new QtTimelineCanvas;
    scrollArea_->setWidget(canvas_);
    scrollArea_->setWindowFlag(Qt::FramelessWindowHint, true);
    scrollArea_->setAttribute(Qt::WA_NativeWindow);

    const HWND qtWindowHandle = reinterpret_cast<HWND>(scrollArea_->winId());
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
    scrollArea_->show();
    return true;
}

void QtTimelineCanvasHost::resize(const CRect &bounds)
{
    if (scrollArea_)
        ::SetWindowPos(reinterpret_cast<HWND>(scrollArea_->winId()), nullptr,
                       bounds.left, bounds.top, bounds.Width(), bounds.Height(),
                       SWP_NOZORDER | SWP_NOACTIVATE);
}

void QtTimelineCanvasHost::setPresentationState(
    const TimelinePresentationState &state)
{
    if (canvas_)
        canvas_->setPresentationState(state);
}

void QtTimelineCanvasHost::setSeekHandler(SeekHandler handler)
{
    if (canvas_)
        canvas_->setSeekHandler(std::move(handler));
}

void QtTimelineCanvasHost::setTimelineClipEditedHandler(TimelineClipEditedHandler handler)
{
    if (canvas_)
        canvas_->setTimelineClipEditedHandler(std::move(handler));
}

void QtTimelineCanvasHost::setMediaAssetDroppedHandler(MediaAssetDroppedHandler handler)
{
    if (canvas_)
        canvas_->setMediaAssetDroppedHandler(std::move(handler));
}

void QtTimelineCanvasHost::setAssetPresentationResolver(AssetPresentationResolver resolver)
{
    if (canvas_)
        canvas_->setAssetPresentationResolver(std::move(resolver));
}

void QtTimelineCanvasHost::setClipThumbnailResolver(ClipThumbnailResolver resolver)
{
    if (canvas_)
        canvas_->setClipThumbnailResolver(std::move(resolver));
}

void QtTimelineCanvasHost::setAudioWaveformResolver(
    AudioWaveformResolver resolver)
{
    if (canvas_)
        canvas_->setAudioWaveformResolver(std::move(resolver));
}

void QtTimelineCanvasHost::setTimelineClipDeletedHandler(TimelineClipDeletedHandler handler)
{
    if (canvas_)
        canvas_->setTimelineClipDeletedHandler(std::move(handler));
}

void QtTimelineCanvasHost::setTimelineClipSelectedHandler(TimelineClipSelectedHandler handler)
{
    if (canvas_)
        canvas_->setTimelineClipSelectedHandler(std::move(handler));
}

void QtTimelineCanvasHost::setTimelineFocusRequestedHandler(
    TimelineFocusRequestedHandler handler)
{
    if (canvas_)
        canvas_->setTimelineFocusRequestedHandler(std::move(handler));
}

void QtTimelineCanvasHost::setAudioTrackVisibilityHandler(
    AudioTrackVisibilityHandler handler)
{
    if (canvas_)
        canvas_->setAudioTrackVisibilityHandler(std::move(handler));
}
