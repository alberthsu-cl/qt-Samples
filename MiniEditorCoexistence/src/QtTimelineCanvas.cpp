#include "QtTimelineCanvas.h"

#include <QMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>

#include <algorithm>

namespace {

constexpr int kTimelineLeft = 76;
constexpr int kRulerHeight = 30;
constexpr int kTrackHeight = 66;
constexpr int kCanvasHeight = kRulerHeight + kTrackHeight * 2 + 16;
constexpr int kTimelineFramesPerScaleUnit = 300;
constexpr int kTimelineMaximumFrame = 600;
constexpr int kTimelinePixelsAt100Percent = 334;
constexpr int kFramesPerSecond = 30;
constexpr char kMediaAssetMimeType[] = "application/x-mini-editor-media-id";

int pixelsPerScaleUnit(const TimelineViewState &state)
{
    return std::max(1, kTimelinePixelsAt100Percent * state.zoomPercent / 100);
}

QString timeLabelForFrame(int frame)
{
    const int totalSeconds = frame / kFramesPerSecond;
    return QStringLiteral("%1:%2")
        .arg(totalSeconds / 60, 2, 10, QLatin1Char('0'))
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

QtTimelineCanvas::QtTimelineCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(kCanvasHeight);
    setAutoFillBackground(false);
}

void QtTimelineCanvas::setSelectedAssetIndex(int selectedAssetIndex)
{
    // The current timeline renderer resolves clips by stable media ID. Keep
    // this legacy selection value only for compatibility with the host API.
    selectedAssetIndex_ = std::max(0, selectedAssetIndex);
    update();
}

void QtTimelineCanvas::setClipSettings(const ClipSettings &settings)
{
    clipSettings_ = settings;
    update();
}

void QtTimelineCanvas::setTimelineClipState(const TimelineClipState &state)
{
    timelineClipState_ = state;
    if (!isDraggingClip_)
        dragPreviewState_ = state;
    update();
}

void QtTimelineCanvas::setPlaybackState(const PlaybackState &state)
{
    playbackState_ = state;
    update();
}

void QtTimelineCanvas::setViewState(const TimelineViewState &state)
{
    viewState_ = state;
    setTimelineDuration(timelineDurationFrames_);
    update();
}

void QtTimelineCanvas::setSeekHandler(SeekHandler handler)
{
    seekHandler_ = std::move(handler);
}

void QtTimelineCanvas::setTimelineClipEditedHandler(TimelineClipEditedHandler handler)
{
    timelineClipEditedHandler_ = std::move(handler);
}

void QtTimelineCanvas::setTimelineClips(const std::vector<TimelineClip> &clips)
{
    timelineClips_ = clips;
    update();
}

void QtTimelineCanvas::setSelectedClipId(int clipId)
{
    selectedClipId_ = clipId;
    update();
}

void QtTimelineCanvas::setTimelineDuration(int durationFrames)
{
    timelineDurationFrames_ = std::max(600, durationFrames);
    const int contentWidth = kTimelineLeft
        + timelineDurationFrames_ * pixelsPerScaleUnit(viewState_)
            / kTimelineFramesPerScaleUnit + 24;
    setMinimumWidth(contentWidth);
    update();
}

void QtTimelineCanvas::setMediaAssetDroppedHandler(MediaAssetDroppedHandler handler)
{
    mediaAssetDroppedHandler_ = std::move(handler);
}

void QtTimelineCanvas::setAssetPresentationResolver(AssetPresentationResolver resolver)
{
    assetPresentationResolver_ = std::move(resolver);
    update();
}

void QtTimelineCanvas::setTimelineClipDeletedHandler(TimelineClipDeletedHandler handler)
{
    timelineClipDeletedHandler_ = std::move(handler);
}

void QtTimelineCanvas::setTimelineClipSelectedHandler(TimelineClipSelectedHandler handler)
{
    timelineClipSelectedHandler_ = std::move(handler);
}

void QtTimelineCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 32, 37));
    painter.fillRect(0, 0, width(), kRulerHeight, QColor(28, 30, 35));
    painter.fillRect(0, kRulerHeight, width(), kTrackHeight, QColor(43, 46, 54));
    painter.fillRect(0, kRulerHeight + kTrackHeight + 8,
                     width(), kTrackHeight, QColor(38, 41, 48));

    const int tickFrames = viewState_.zoomPercent < 75 ? 120 : 60;
    painter.setPen(QColor(190, 195, 205));
    for (int frame = 0; frame <= timelineDurationFrames_; frame += tickFrames) {
        const int x = kTimelineLeft + frame * pixelsPerScaleUnit(viewState_)
            / kTimelineFramesPerScaleUnit;
        painter.drawLine(x, 18, x, kRulerHeight);
        painter.drawText(x + 3, 16, timeLabelForFrame(frame));
    }

    painter.drawText(12, kRulerHeight + 38, QStringLiteral("V1"));
    painter.drawText(12, kRulerHeight + kTrackHeight + 46, QStringLiteral("A1"));

    const auto drawClip = [&painter, this](const TimelineClip &sourceClip) {
        TimelineClip clip = sourceClip;
        if (isDraggingClip_ && clip.id == dragClipId_)
            clip.state = dragPreviewState_;
        if (!assetPresentationResolver_)
            return;
        const std::optional<TimelineAssetPresentation> presentation =
            assetPresentationResolver_(clip.mediaAssetId);
        if (!presentation)
            return;

        const QColor assetColor = presentation->color.darker(100);
        const int pixels = pixelsPerScaleUnit(viewState_);
        const int left = kTimelineLeft + clip.state.startFrame * pixels
            / kTimelineFramesPerScaleUnit;
        const int clipWidth = std::max(1, clip.state.durationFrames * pixels
            / kTimelineFramesPerScaleUnit);
        const int trackTop = clip.trackType == TimelineTrackType::Audio
            ? kRulerHeight + kTrackHeight + 8 : kRulerHeight;
        const QRect clipRect(left, trackTop + 5, clipWidth, kTrackHeight - 10);
        painter.fillRect(clipRect, assetColor);
        const bool selected = clip.id == selectedClipId_;
        painter.setPen(QPen(selected ? QColor(255, 196, 72) : QColor(180, 220, 255),
                            selected ? 3 : 1));
        painter.drawRect(clipRect.adjusted(selected ? 1 : 0,
                                           selected ? 1 : 0,
                                           selected ? -2 : -1,
                                           selected ? -2 : -1));
        painter.setPen(Qt::white);
        painter.drawText(clipRect.adjusted(10, 0, -10, 0),
                         Qt::AlignCenter | Qt::TextSingleLine,
                         presentation->displayName);
    };
    if (!timelineClips_.empty()) {
        for (const TimelineClip &clip : timelineClips_)
            drawClip(clip);
    } else {
        painter.setPen(QColor(166, 171, 183));
        painter.drawText(QRect(kTimelineLeft, kRulerHeight,
                               width() - kTimelineLeft - 12, kTrackHeight),
                         Qt::AlignCenter, QStringLiteral("Drag media here to begin editing"));
    }

    if (!viewState_.isAudioTrackVisible)
        painter.drawText(QRect(kTimelineLeft, kRulerHeight + kTrackHeight + 8,
                               width() - kTimelineLeft - 12, kTrackHeight),
                         Qt::AlignVCenter, QStringLiteral("Audio track hidden"));

    if (isMediaDropPreviewVisible_) {
        const int pixels = pixelsPerScaleUnit(viewState_);
        const int left = kTimelineLeft + mediaDropStartFrame_ * pixels
            / kTimelineFramesPerScaleUnit;
        const int previewWidth = std::max(
            1, mediaDropPresentation_.durationFrames * pixels
                / kTimelineFramesPerScaleUnit);
        const int trackTop = mediaDropPresentation_.trackType == TimelineTrackType::Audio
            ? kRulerHeight + kTrackHeight + 8 : kRulerHeight;
        const QRect previewRect(left, trackTop + 5,
                                previewWidth, kTrackHeight - 10);

        QColor previewColor = mediaDropPresentation_.color;
        previewColor.setAlpha(150);
        painter.fillRect(previewRect, previewColor);
        painter.setPen(QPen(QColor(108, 190, 255), 2, Qt::DashLine));
        painter.drawRect(previewRect.adjusted(0, 0, -1, -1));

        painter.setPen(Qt::white);
        painter.drawText(previewRect.adjusted(8, 0, -8, 0),
                         Qt::AlignCenter | Qt::TextSingleLine,
                         mediaDropPresentation_.displayName);

        // The guide and label make the placement rule unambiguous: this
        // left edge is the exact frame at which the new clip will start.
        painter.fillRect(left - 1, 0, 3, height(), QColor(108, 190, 255));
        const QString startLabel = QStringLiteral("Start %1")
            .arg(timeLabelForFrame(mediaDropStartFrame_));
        const QRect labelRect(left + 5, 2, 82, 22);
        painter.fillRect(labelRect, QColor(18, 20, 24, 230));
        painter.setPen(QColor(220, 235, 250));
        painter.drawText(labelRect, Qt::AlignCenter, startLabel);
    }

    const int playheadX = kTimelineLeft + playbackState_.currentFrame
        * pixelsPerScaleUnit(viewState_) / kTimelineFramesPerScaleUnit;
    painter.fillRect(playheadX, 0, 2, height(), QColor(240, 74, 74));
}

void QtTimelineCanvas::mousePressEvent(QMouseEvent *event)
{
    const QPoint point = event->position().toPoint();
    if (point.y() < kRulerHeight && seekHandler_) {
        seekHandler_(frameAtRulerX(point.x()));
    } else if (const TimelineClip *clip = clipAt(point)) {
        selectedClipId_ = clip->id;
        if (timelineClipSelectedHandler_)
            timelineClipSelectedHandler_(clip->id);
        isDraggingClip_ = true;
        dragClipId_ = clip->id;
        dragPreviewState_ = clip->state;
        dragFrameOffset_ = frameAtTimelineX(point.x()) - dragPreviewState_.startFrame;
        grabMouse();
    }
    QWidget::mousePressEvent(event);
}

void QtTimelineCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (isDraggingClip_ && (event->buttons() & Qt::LeftButton)) {
        dragPreviewState_.startFrame = std::max(
            0, frameAtTimelineX(event->position().x()) - dragFrameOffset_);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void QtTimelineCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (isDraggingClip_ && event->button() == Qt::LeftButton) {
        dragPreviewState_.startFrame = std::max(
            0, frameAtTimelineX(event->position().x()) - dragFrameOffset_);
        releaseMouse();
        isDraggingClip_ = false;
        if (timelineClipEditedHandler_)
            timelineClipEditedHandler_(dragClipId_, dragPreviewState_);
        dragClipId_ = 0;
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void QtTimelineCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete && selectedClipId_ != 0
        && timelineClipDeletedHandler_) {
        timelineClipDeletedHandler_(selectedClipId_);
        selectedClipId_ = 0;
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void QtTimelineCanvas::dragEnterEvent(QDragEnterEvent *event)
{
    if (updateMediaDropPreview(event->mimeData(), event->position().toPoint().x()))
        event->acceptProposedAction();
    else {
        clearMediaDropPreview();
        event->ignore();
    }
}

void QtTimelineCanvas::dragMoveEvent(QDragMoveEvent *event)
{
    if (updateMediaDropPreview(event->mimeData(), event->position().toPoint().x())) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    } else {
        clearMediaDropPreview();
        event->ignore();
    }
}

void QtTimelineCanvas::dragLeaveEvent(QDragLeaveEvent *event)
{
    clearMediaDropPreview();
    event->accept();
}

void QtTimelineCanvas::dropEvent(QDropEvent *event)
{
    if (!updateMediaDropPreview(event->mimeData(), event->position().toPoint().x())) {
        event->ignore();
        return;
    }

    const int mediaAssetId = mediaDropAssetId_;
    const int startFrame = mediaDropStartFrame_;
    clearMediaDropPreview();
    if (mediaAssetDroppedHandler_)
        mediaAssetDroppedHandler_(mediaAssetId, startFrame);
    event->setDropAction(Qt::CopyAction);
    event->accept();
}

bool QtTimelineCanvas::updateMediaDropPreview(const QMimeData *mimeData, int timelineX)
{
    if (mimeData == nullptr
        || !mimeData->hasFormat(QString::fromLatin1(kMediaAssetMimeType))
        || !assetPresentationResolver_) {
        return false;
    }

    bool isValidId = false;
    const int mediaAssetId = QString::fromLatin1(
        mimeData->data(QString::fromLatin1(kMediaAssetMimeType))).toInt(&isValidId);
    if (!isValidId)
        return false;

    const std::optional<TimelineAssetPresentation> presentation =
        assetPresentationResolver_(mediaAssetId);
    if (!presentation || presentation->durationFrames <= 0)
        return false;

    isMediaDropPreviewVisible_ = true;
    mediaDropAssetId_ = mediaAssetId;
    mediaDropStartFrame_ = frameAtTimelineX(timelineX);
    mediaDropPresentation_ = *presentation;
    update();
    return true;
}

void QtTimelineCanvas::clearMediaDropPreview()
{
    if (!isMediaDropPreviewVisible_)
        return;

    isMediaDropPreviewVisible_ = false;
    mediaDropAssetId_ = 0;
    update();
}

int QtTimelineCanvas::frameAtRulerX(int x) const
{
    const int clipWidth = pixelsPerScaleUnit(viewState_);
    const int relativeX = std::clamp(x - kTimelineLeft, 0, clipWidth);
    return std::clamp(relativeX * kTimelineFramesPerScaleUnit / clipWidth,
                      0, kTimelineFramesPerScaleUnit - 1);
}

int QtTimelineCanvas::frameAtTimelineX(int x) const
{
    return std::clamp((x - kTimelineLeft) * kTimelineFramesPerScaleUnit
                          / pixelsPerScaleUnit(viewState_),
                      0, timelineDurationFrames_);
}

QRect QtTimelineCanvas::timelineClipRect(const TimelineClip &clip) const
{
    const TimelineClipState &clipState = isDraggingClip_ && clip.id == dragClipId_
        ? dragPreviewState_ : clip.state;
    const int pixels = pixelsPerScaleUnit(viewState_);
    const int left = kTimelineLeft + clipState.startFrame * pixels / kTimelineFramesPerScaleUnit;
    const int clipWidth = std::max(1, clipState.durationFrames * pixels
        / kTimelineFramesPerScaleUnit);
    const int trackTop = clip.trackType == TimelineTrackType::Audio
        ? kRulerHeight + kTrackHeight + 8 : kRulerHeight;
    return QRect(left, trackTop + 5, clipWidth, kTrackHeight - 10);
}

const TimelineClip *QtTimelineCanvas::clipAt(const QPoint &point) const
{
    for (const TimelineClip &clip : timelineClips_) {
        if (timelineClipRect(clip).contains(point))
            return &clip;
    }
    return nullptr;
}
