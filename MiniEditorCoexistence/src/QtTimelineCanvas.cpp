#include "QtTimelineCanvas.h"

#include "DemoProject.h"

#include <QMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QDragEnterEvent>
#include <QDropEvent>

#include <algorithm>

namespace {

constexpr int kTimelineLeft = 76;
constexpr int kRulerHeight = 30;
constexpr int kTrackHeight = 66;
constexpr int kTimelineFramesPerScaleUnit = 300;
constexpr int kTimelineMaximumFrame = 600;
constexpr int kTimelinePixelsAt100Percent = 334;
constexpr char kMediaAssetMimeType[] = "application/x-mini-editor-media-index";

int pixelsPerScaleUnit(const TimelineViewState &state)
{
    return std::max(1, kTimelinePixelsAt100Percent * state.zoomPercent / 100);
}

} // namespace

QtTimelineCanvas::QtTimelineCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setAutoFillBackground(false);
}

void QtTimelineCanvas::setSelectedAssetIndex(int selectedAssetIndex)
{
    selectedAssetIndex_ = std::clamp(selectedAssetIndex, 0,
                                     static_cast<int>(demoAssets().size()) - 1);
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

void QtTimelineCanvas::setMediaAssetDroppedHandler(MediaAssetDroppedHandler handler)
{
    mediaAssetDroppedHandler_ = std::move(handler);
}

void QtTimelineCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 32, 37));
    painter.fillRect(0, 0, width(), kRulerHeight, QColor(28, 30, 35));
    painter.fillRect(0, kRulerHeight, width(), kTrackHeight, QColor(43, 46, 54));
    painter.fillRect(0, kRulerHeight + kTrackHeight + 8,
                     width(), kTrackHeight, QColor(38, 41, 48));

    const int tickSpacing = std::max(30, 80 * viewState_.zoomPercent / 100);
    painter.setPen(QColor(190, 195, 205));
    for (int x = 110; x < width(); x += tickSpacing) {
        painter.drawLine(x, 18, x, kRulerHeight);
        painter.drawText(x + 3, 16, QString::number((x - 110) * 10 / tickSpacing));
    }

    painter.drawText(12, kRulerHeight + 38, QStringLiteral("V1"));
    painter.drawText(12, kRulerHeight + kTrackHeight + 46, QStringLiteral("A1"));

    const auto drawClip = [&painter, this](const TimelineClip &clip) {
        const auto &asset = demoAssets()[std::clamp(clip.mediaAssetIndex, 0,
                                                     static_cast<int>(demoAssets().size()) - 1)];
        const QColor assetColor = QColor::fromRgb(asset.thumbnailColor).darker(100);
        const int pixels = pixelsPerScaleUnit(viewState_);
        const int left = kTimelineLeft + clip.state.startFrame * pixels
            / kTimelineFramesPerScaleUnit;
        const int clipWidth = std::max(1, clip.state.durationFrames * pixels
            / kTimelineFramesPerScaleUnit);
        const int trackTop = clip.trackType == TimelineTrackType::Audio
            ? kRulerHeight + kTrackHeight + 8 : kRulerHeight;
        const QRect clipRect(left, trackTop + 5, clipWidth, kTrackHeight - 10);
        painter.fillRect(clipRect, assetColor);
        painter.setPen(QColor(180, 220, 255));
        painter.drawRect(clipRect.adjusted(0, 0, -1, -1));
        painter.setPen(Qt::white);
        painter.drawText(clipRect.adjusted(10, 0, -10, 0),
                         Qt::AlignCenter | Qt::TextSingleLine,
                         QString::fromWCharArray(asset.name));
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

    const int playheadX = kTimelineLeft + playbackState_.currentFrame
        * pixelsPerScaleUnit(viewState_) / kTimelineFramesPerScaleUnit;
    painter.fillRect(playheadX, 0, 2, height(), QColor(240, 74, 74));
}

void QtTimelineCanvas::mousePressEvent(QMouseEvent *event)
{
    const QPoint point = event->position().toPoint();
    if (point.y() < kRulerHeight && seekHandler_) {
        seekHandler_(frameAtRulerX(point.x()));
    } else if (timelineClipRect().contains(point)) {
        isDraggingClip_ = true;
        dragPreviewState_ = timelineClipState_;
        dragFrameOffset_ = frameAtTimelineX(point.x()) - dragPreviewState_.startFrame;
        grabMouse();
    }
    QWidget::mousePressEvent(event);
}

void QtTimelineCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (isDraggingClip_ && (event->buttons() & Qt::LeftButton)) {
        dragPreviewState_.startFrame = std::clamp(
            frameAtTimelineX(event->position().x()) - dragFrameOffset_, 0,
            kTimelineMaximumFrame - dragPreviewState_.durationFrames);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void QtTimelineCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (isDraggingClip_ && event->button() == Qt::LeftButton) {
        dragPreviewState_.startFrame = std::clamp(
            frameAtTimelineX(event->position().x()) - dragFrameOffset_, 0,
            kTimelineMaximumFrame - dragPreviewState_.durationFrames);
        releaseMouse();
        isDraggingClip_ = false;
        if (timelineClipEditedHandler_)
            timelineClipEditedHandler_(dragPreviewState_);
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void QtTimelineCanvas::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(kMediaAssetMimeType)))
        event->acceptProposedAction();
    else
        event->ignore();
}

void QtTimelineCanvas::dropEvent(QDropEvent *event)
{
    const QByteArray data = event->mimeData()->data(QString::fromLatin1(kMediaAssetMimeType));
    bool ok = false;
    const int mediaAssetIndex = QString::fromLatin1(data).toInt(&ok);
    if (ok && mediaAssetDroppedHandler_)
        mediaAssetDroppedHandler_(mediaAssetIndex, frameAtTimelineX(event->position().x()));
    event->setDropAction(Qt::CopyAction);
    event->accept();
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
                      0, kTimelineMaximumFrame);
}

QRect QtTimelineCanvas::timelineClipRect() const
{
    const TimelineClipState &clipState = isDraggingClip_ ? dragPreviewState_ : timelineClipState_;
    const int pixels = pixelsPerScaleUnit(viewState_);
    const int left = kTimelineLeft + clipState.startFrame * pixels / kTimelineFramesPerScaleUnit;
    const int clipWidth = std::max(1, clipState.durationFrames * pixels
        / kTimelineFramesPerScaleUnit);
    return QRect(left, kRulerHeight + 5, clipWidth, kTrackHeight - 10);
}
