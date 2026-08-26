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

constexpr int kFramesPerSecond = 30;
constexpr int kTrimHandleWidth = 7;
constexpr char kMediaAssetMimeType[] = "application/x-mini-editor-media-id";

QRect toQRect(const TimelineRectangle &rectangle)
{
    return { rectangle.left, rectangle.top, rectangle.width, rectangle.height };
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
    setMinimumHeight(TimelineGeometry::kCanvasHeight);
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
    if (!isEditingClip())
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
    setMinimumWidth(geometry().contentWidth());
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
    const TimelineGeometry timelineGeometry = geometry();
    const QRect videoTrackRect = toQRect(timelineGeometry.trackRectangle(
        TimelineTrackType::Video, width()));
    const QRect audioTrackRect = toQRect(timelineGeometry.trackRectangle(
        TimelineTrackType::Audio, width()));

    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 32, 37));
    painter.fillRect(0, 0, width(), TimelineGeometry::kRulerHeight,
                     QColor(28, 30, 35));
    painter.fillRect(videoTrackRect, QColor(43, 46, 54));
    painter.fillRect(audioTrackRect, QColor(38, 41, 48));

    const int tickFrames = viewState_.zoomPercent < 75 ? 120 : 60;
    painter.setPen(QColor(190, 195, 205));
    for (int frame = 0; frame <= timelineDurationFrames_; frame += tickFrames) {
        const int x = timelineGeometry.xForFrame(frame);
        painter.drawLine(x, 18, x, TimelineGeometry::kRulerHeight);
        painter.drawText(x + 3, 16, timeLabelForFrame(frame));
    }

    painter.drawText(12, videoTrackRect.top() + 38, QStringLiteral("V1"));
    painter.drawText(12, audioTrackRect.top() + 38, QStringLiteral("A1"));

    const auto drawClip = [&painter, this, &timelineGeometry](const TimelineClip &sourceClip) {
        TimelineClip clip = sourceClip;
        if (isEditingClip() && clip.id == dragClipId_)
            clip.state = dragPreviewState_;
        if (!assetPresentationResolver_)
            return;
        const std::optional<TimelineAssetPresentation> presentation =
            assetPresentationResolver_(clip.mediaAssetId);
        if (!presentation)
            return;

        const QColor assetColor = presentation->color.darker(100);
        const QRect clipRect = toQRect(timelineGeometry.clipRectangle(clip));
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

        if (selected) {
            const int handleWidth = std::min(
                kTrimHandleWidth, std::max(1, clipRect.width() / 2));
            const QRect startHandle(clipRect.left(), clipRect.top(),
                                    handleWidth, clipRect.height());
            const QRect endHandle(clipRect.right() - handleWidth + 1,
                                  clipRect.top(), handleWidth, clipRect.height());
            const QColor handleColor(255, 196, 72, 190);
            painter.fillRect(startHandle, handleColor);
            painter.fillRect(endHandle, handleColor);
            painter.setPen(QColor(255, 238, 190));
            painter.drawLine(startHandle.right(), startHandle.top() + 7,
                             startHandle.right(), startHandle.bottom() - 7);
            painter.drawLine(endHandle.left(), endHandle.top() + 7,
                             endHandle.left(), endHandle.bottom() - 7);

            if (isEditingClip() && clip.id == dragClipId_) {
                const QString rangeText = dragTrimContext_.mediaKind == MediaKind::Image
                    ? QStringLiteral("Start %1f  |  Display %2f")
                          .arg(clip.state.startFrame)
                          .arg(clip.state.durationFrames)
                    : QStringLiteral("Source %1f-%2f  |  Duration %3f")
                          .arg(clip.state.sourceInFrame)
                          .arg(clip.state.sourceInFrame + clip.state.durationFrames)
                          .arg(clip.state.durationFrames);
                const QRect rangeRect(clipRect.left() + 3, 2, 230, 22);
                painter.fillRect(rangeRect, QColor(18, 20, 24, 235));
                painter.setPen(QColor(230, 238, 248));
                painter.drawText(rangeRect, Qt::AlignCenter, rangeText);
            }
        }
    };
    if (!timelineClips_.empty()) {
        for (const TimelineClip &clip : timelineClips_)
            drawClip(clip);
    } else {
        painter.setPen(QColor(166, 171, 183));
        painter.drawText(QRect(TimelineGeometry::kTimelineLeft, videoTrackRect.top(),
                               width() - TimelineGeometry::kTimelineLeft - 12,
                               videoTrackRect.height()),
                         Qt::AlignCenter, QStringLiteral("Drag media here to begin editing"));
    }

    if (!viewState_.isAudioTrackVisible)
        painter.drawText(QRect(TimelineGeometry::kTimelineLeft, audioTrackRect.top(),
                               width() - TimelineGeometry::kTimelineLeft - 12,
                               audioTrackRect.height()),
                         Qt::AlignVCenter, QStringLiteral("Audio track hidden"));

    if (isMediaDropPreviewVisible_) {
        const TimelineClipState previewState{
            mediaDropStartFrame_, mediaDropPresentation_.durationFrames
        };
        const QRect previewRect = toQRect(timelineGeometry.clipRectangle(
            mediaDropPresentation_.trackType, previewState));
        const int left = previewRect.left();

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

    const int playheadX = timelineGeometry.xForFrame(playbackState_.currentFrame);
    painter.fillRect(playheadX, 0, 2, height(), QColor(240, 74, 74));
}

void QtTimelineCanvas::mousePressEvent(QMouseEvent *event)
{
    const QPoint point = event->position().toPoint();
    const TimelineGeometry timelineGeometry = geometry();
    if (point.y() < TimelineGeometry::kRulerHeight && seekHandler_) {
        seekHandler_(timelineGeometry.rulerFrameAtX(point.x()));
    } else if (event->button() == Qt::LeftButton) {
        const TimelineClipHit hit = timelineGeometry.hitTestClip(
            timelineClips_, { point.x(), point.y() },
            selectedClipId_, kTrimHandleWidth);
        if (hit.clip == nullptr) {
            QWidget::mousePressEvent(event);
            return;
        }

        selectedClipId_ = hit.clip->id;
        if (timelineClipSelectedHandler_)
            timelineClipSelectedHandler_(hit.clip->id);
        dragRegion_ = hit.region;
        dragClipId_ = hit.clip->id;
        dragOriginalState_ = hit.clip->state;
        dragPreviewState_ = hit.clip->state;
        dragTrimContext_ = { MediaKind::Video,
                             hit.clip->state.sourceInFrame
                                 + hit.clip->state.durationFrames };
        if (assetPresentationResolver_) {
            const std::optional<TimelineAssetPresentation> presentation =
                assetPresentationResolver_(hit.clip->mediaAssetId);
            if (presentation) {
                dragTrimContext_ = { presentation->mediaKind,
                                     presentation->durationFrames };
            }
        }
        dragFrameOffset_ = timelineGeometry.frameAtX(point.x())
            - dragPreviewState_.startFrame;
        setCursor(dragRegion_ == TimelineClipHitRegion::Body
                      ? Qt::ClosedHandCursor : Qt::SizeHorCursor);
        grabMouse();
    }
    QWidget::mousePressEvent(event);
}

void QtTimelineCanvas::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint point = event->position().toPoint();
    if (isEditingClip() && (event->buttons() & Qt::LeftButton)) {
        updateDragPreview(point.x());
        update();
    } else {
        updateMouseCursor(point);
    }
    QWidget::mouseMoveEvent(event);
}

void QtTimelineCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (isEditingClip() && event->button() == Qt::LeftButton) {
        const QPoint point = event->position().toPoint();
        updateDragPreview(point.x());
        releaseMouse();
        dragRegion_ = TimelineClipHitRegion::None;
        if (timelineClipEditedHandler_)
            timelineClipEditedHandler_(dragClipId_, dragPreviewState_);
        dragClipId_ = 0;
        setMinimumWidth(geometry().contentWidth());
        updateMouseCursor(point);
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
    mediaDropStartFrame_ = geometry().frameAtX(timelineX);
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

TimelineGeometry QtTimelineCanvas::geometry() const
{
    return { viewState_.zoomPercent, timelineDurationFrames_ };
}

bool QtTimelineCanvas::isEditingClip() const
{
    return dragRegion_ != TimelineClipHitRegion::None;
}

void QtTimelineCanvas::updateDragPreview(int timelineX)
{
    const TimelineGeometry timelineGeometry = geometry();
    const bool isTrimming = dragRegion_ == TimelineClipHitRegion::TrimStart
        || dragRegion_ == TimelineClipHitRegion::TrimEnd;
    const int frame = isTrimming
        ? timelineGeometry.frameAtXUnclamped(timelineX)
        : timelineGeometry.frameAtX(timelineX);
    switch (dragRegion_) {
    case TimelineClipHitRegion::Body:
        dragPreviewState_ = TimelineClipEdit::moveTo(
            dragOriginalState_, frame - dragFrameOffset_);
        break;
    case TimelineClipHitRegion::TrimStart:
        dragPreviewState_ = TimelineClipEdit::trimStartTo(
            dragOriginalState_, frame, dragTrimContext_);
        break;
    case TimelineClipHitRegion::TrimEnd:
        dragPreviewState_ = TimelineClipEdit::trimEndTo(
            dragOriginalState_, frame, dragTrimContext_);
        break;
    case TimelineClipHitRegion::None:
        break;
    }

    const int provisionalEnd = dragPreviewState_.startFrame
        + dragPreviewState_.durationFrames;
    if (provisionalEnd > timelineDurationFrames_) {
        setMinimumWidth(TimelineGeometry(viewState_.zoomPercent, provisionalEnd)
                            .contentWidth());
    }
}

void QtTimelineCanvas::updateMouseCursor(const QPoint &point)
{
    const TimelineClipHit hit = geometry().hitTestClip(
        timelineClips_, { point.x(), point.y() }, selectedClipId_, kTrimHandleWidth);
    if (hit.region == TimelineClipHitRegion::TrimStart
        || hit.region == TimelineClipHitRegion::TrimEnd) {
        setCursor(Qt::SizeHorCursor);
    } else if (hit.region == TimelineClipHitRegion::Body) {
        setCursor(Qt::OpenHandCursor);
    } else {
        unsetCursor();
    }
}
