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

void QtTimelineCanvasHost::setSelectedAssetIndex(int selectedAssetIndex)
{
    if (canvas_)
        canvas_->setSelectedAssetIndex(selectedAssetIndex);
}

void QtTimelineCanvasHost::setClipSettings(const ClipSettings &settings)
{
    if (canvas_)
        canvas_->setClipSettings(settings);
}

void QtTimelineCanvasHost::setTimelineClipState(const TimelineClipState &state)
{
    if (canvas_)
        canvas_->setTimelineClipState(state);
}

void QtTimelineCanvasHost::setPlaybackState(const PlaybackState &state)
{
    if (canvas_)
        canvas_->setPlaybackState(state);
}

void QtTimelineCanvasHost::setViewState(const TimelineViewState &state)
{
    if (canvas_)
        canvas_->setViewState(state);
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

void QtTimelineCanvasHost::setTimelineClips(const std::vector<TimelineClip> &clips)
{
    if (canvas_)
        canvas_->setTimelineClips(clips);
}

void QtTimelineCanvasHost::setSelectedClipId(int clipId)
{
    if (canvas_)
        canvas_->setSelectedClipId(clipId);
}

void QtTimelineCanvasHost::setTimelineDuration(int durationFrames)
{
    if (canvas_)
        canvas_->setTimelineDuration(durationFrames);
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
