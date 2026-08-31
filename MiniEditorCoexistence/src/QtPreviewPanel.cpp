#include "QtPreviewPanel.h"

#include "QtPreviewEffectPipeline.h"

#include <QImage>
#include <QPainter>
#include <QStringList>
#include <QVideoFrame>
#include <QVideoSink>

#include <algorithm>

namespace {

QString clipPositionText(ClipPosition position)
{
    return QString::fromWCharArray(clipPositionDisplayName(position));
}

QRect videoRectangle(const QRect &availableRect, const ClipSettings &settings)
{
    const int baseVideoHeight = std::min(availableRect.height(),
                                         availableRect.width() * 9 / 16);
    const int videoHeight = std::max(1, std::min(availableRect.height(),
        baseVideoHeight * settings.scalePercent / 100));
    const int videoWidth = std::max(1, std::min(availableRect.width(),
        videoHeight * 16 / 9));

    int videoLeft = availableRect.left() + (availableRect.width() - videoWidth) / 2;
    int videoTop = availableRect.top() + (availableRect.height() - videoHeight) / 2;
    switch (settings.position) {
    case ClipPosition::TopLeft:
        videoLeft = availableRect.left();
        videoTop = availableRect.top();
        break;
    case ClipPosition::TopRight:
        videoLeft = availableRect.right() - videoWidth + 1;
        videoTop = availableRect.top();
        break;
    case ClipPosition::BottomLeft:
        videoLeft = availableRect.left();
        videoTop = availableRect.bottom() - videoHeight + 1;
        break;
    case ClipPosition::BottomRight:
        videoLeft = availableRect.right() - videoWidth + 1;
        videoTop = availableRect.bottom() - videoHeight + 1;
        break;
    case ClipPosition::Center:
        break;
    }

    return { videoLeft, videoTop, videoWidth, videoHeight };
}

} // namespace

QtPreviewPanel::QtPreviewPanel(QWidget *parent)
    : QWidget(parent)
    , videoSink_(new QVideoSink(this))
    , effectPipeline_(new QtPreviewEffectPipeline(this))
{
    setMinimumSize(200, 150);
    setAutoFillBackground(false);

    connect(videoSink_, &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) {
                decodedVideoFrame_ = frame;
                submitFrameForProcessing(frame.toImage());
                update();
            });
    connect(effectPipeline_, &QtPreviewEffectPipeline::frameProcessed, this,
            [this](const QImage &result) {
                processedImage_ = result;
                update();
            });
}

void QtPreviewPanel::setPreviewState(const PreviewState &state)
{
    const bool processingInputChanged =
        previewState_.mediaAssetId != state.mediaAssetId
        || previewState_.mediaKind != state.mediaKind
        || previewState_.settings.effect != state.settings.effect
        || previewState_.settings.effectIntensityPercent
            != state.settings.effectIntensityPercent;
    previewState_ = state;
    if (processingInputChanged) {
        // Invalidate any result still being produced for the old clip/source.
        // The pipeline will ignore that result when it eventually arrives.
        processedImage_ = QImage();
        effectPipeline_->clear();
        submitFrameForProcessing(sourceImageToPaint());
    }
    update();
}

void QtPreviewPanel::setPlaybackState(const PlaybackState &state)
{
    playbackState_ = state;
    update();
}

void QtPreviewPanel::setStillImage(const QImage &image)
{
    // MainFrame refreshes preview presentation on every playback tick. Do not
    // invalidate a video effect merely because the same empty still-image
    // value was supplied again, or restart still processing for the same image.
    if (stillImage_.cacheKey() == image.cacheKey())
        return;

    stillImage_ = image;
    processedImage_ = QImage();
    effectPipeline_->clear();
    submitFrameForProcessing(image);
    update();
}

QImage QtPreviewPanel::sourceImageToPaint() const
{
    if (previewState_.mediaKind == MediaKind::Image)
        return stillImage_;
    return isDecodedVideoVisible_ ? decodedVideoFrame_.toImage() : QImage();
}

void QtPreviewPanel::submitFrameForProcessing(const QImage &frame)
{
    if (!effectPipeline_->submit(frame, previewState_.settings.effect,
                                 previewState_.settings.effectIntensityPercent)) {
        // A disabled effect must not leave the last processed frame on screen.
        processedImage_ = QImage();
    }
}

QVideoSink *QtPreviewPanel::videoSink() const
{
    return videoSink_;
}

void QtPreviewPanel::setDecodedVideoVisible(bool visible)
{
    isDecodedVideoVisible_ = visible;
    if (!visible) {
        decodedVideoFrame_ = {};
        processedImage_ = QImage();
        effectPipeline_->clear();
    }
    update();
}

void QtPreviewPanel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(37, 39, 45));

    const QRect availableRect(20, 20,
                              std::max(0, width() - 40),
                              std::max(0, height() - 32));
    if (availableRect.width() <= 0 || availableRect.height() <= 0)
        return;

    const ClipSettings &settings = previewState_.settings;
    const QRect videoRect = videoRectangle(availableRect, settings);
    painter.fillRect(availableRect, QColor(24, 26, 30));
    if (!previewState_.hasMedia) {
        painter.setPen(QColor(166, 171, 183));
        painter.drawText(availableRect, Qt::AlignCenter,
                         QStringLiteral("No media at this timeline position"));
    } else {
        const QImage decodedImage = decodedVideoFrame_.toImage();
        const bool canPaintDecodedVideo = isDecodedVideoVisible_
            && previewState_.mediaKind == MediaKind::Video
            && !decodedImage.isNull();
        const bool canPaintStillImage = previewState_.mediaKind == MediaKind::Image
            && !stillImage_.isNull();
        const bool canPaintRealMedia = canPaintDecodedVideo || canPaintStillImage;
        painter.save();
        painter.setOpacity(std::clamp(
            previewState_.effectiveOpacityPercent / 100.0, 0.0, 1.0));
        if (canPaintRealMedia) {
            // The processed frame lags the source by at most one frame while
            // the worker is busy, which is why painting prefers it but never
            // waits for it.
            const QImage &sourceImage = canPaintDecodedVideo ? decodedImage : stillImage_;
            const QImage &image = processedImage_.isNull() ? sourceImage : processedImage_;
            const QSize imageSize = image.size().scaled(
                videoRect.size(), Qt::KeepAspectRatio);
            const QRect imageRect(videoRect.left() + (videoRect.width() - imageSize.width()) / 2,
                                  videoRect.top() + (videoRect.height() - imageSize.height()) / 2,
                                  imageSize.width(), imageSize.height());
            painter.drawImage(imageRect, image);
        } else {
            painter.fillRect(videoRect, QColor::fromRgb(previewState_.thumbnailColorRgb));
        }
        painter.restore();
        if (!canPaintRealMedia) {
            painter.setPen(QPen(QColor(220, 220, 220), 1));
            painter.drawRect(videoRect.adjusted(0, 0, -1, -1));

            painter.setPen(Qt::white);
            painter.drawText(videoRect.adjusted(12, videoRect.height() - 38,
                                                 -12, -12),
                             Qt::AlignCenter | Qt::TextSingleLine,
                             QString::fromStdWString(previewState_.displayName));
        }
        painter.setPen(QColor(166, 171, 183));
        painter.drawText(QRect(availableRect.left(), availableRect.bottom() - 23,
                               availableRect.width(), 22),
                         Qt::AlignCenter | Qt::TextSingleLine,
                         QStringLiteral("Opacity %1%  |  Scale %2%  |  %3")
                             .arg(settings.opacityPercent)
                             .arg(settings.scalePercent)
                             .arg(clipPositionText(settings.position)));
        if (!canPaintRealMedia && previewState_.videoFadeGainPercent < 100) {
            painter.setPen(QColor(255, 205, 120));
            painter.drawText(QRect(availableRect.left(), availableRect.top(),
                                   availableRect.width(), 22),
                             Qt::AlignCenter | Qt::TextSingleLine,
                             QStringLiteral("Fade %1%  ->  %2% opacity")
                                 .arg(previewState_.videoFadeGainPercent)
                                 .arg(previewState_.effectiveOpacityPercent));
        }
    }

    // Real video is already communicating motion through its decoded frames.
    // Keep the frame-time overlay for the sample renderer only; placing it on
    // top of real footage is visual noise during normal playback.
    if (isDecodedVideoVisible_
        || (!playbackState_.isPlaying && !playbackState_.isPaused)) {
        return;
    }

    const QRect overlayBase = previewState_.hasMedia ? videoRect : availableRect;
    const int overlayWidth = std::min(480, std::max(220, overlayBase.width() - 24));
    const int overlayHeight = previewState_.hasAudio ? 96 : 74;
    const QRect overlayRect(
        overlayBase.left() + (overlayBase.width() - overlayWidth) / 2,
        overlayBase.top() + (overlayBase.height() - overlayHeight) / 2,
        overlayWidth, overlayHeight);
    painter.fillRect(overlayRect, QColor(18, 20, 24));
    painter.setPen(QPen(QColor(150, 155, 165), 1));
    painter.drawRect(overlayRect.adjusted(0, 0, -1, -1));

    QStringList overlayLines;
    if (previewState_.mode == PreviewMode::Source) {
        if (previewState_.mediaKind == MediaKind::Image) {
            overlayLines << QStringLiteral("Source preview")
                         << QStringLiteral("Image display frame %1")
                                .arg(playbackState_.currentFrame);
        } else {
            overlayLines << QStringLiteral("Source %1 / %2")
                .arg(frameTimecode(previewState_.sourceFrame,
                                   playbackState_.framesPerSecond))
                .arg(frameTimecode(previewState_.sourceDurationFrames,
                                   playbackState_.framesPerSecond));
        }
    } else {
        overlayLines << QStringLiteral("Timeline %1 / %2")
            .arg(frameTimecode(playbackState_.currentFrame,
                               playbackState_.framesPerSecond))
            .arg(frameTimecode(playbackState_.durationFrames,
                               playbackState_.framesPerSecond));
        if (previewState_.hasMedia) {
            overlayLines << (previewState_.mediaKind == MediaKind::Image
                ? QStringLiteral("Image display frame %1")
                      .arg(previewState_.clipLocalFrame)
                : QStringLiteral("Video source %1 / %2")
                      .arg(frameTimecode(previewState_.sourceFrame,
                                         playbackState_.framesPerSecond))
                      .arg(frameTimecode(previewState_.sourceDurationFrames,
                                         playbackState_.framesPerSecond)));
        } else {
            overlayLines << QStringLiteral("No video at this position");
        }
        if (previewState_.hasAudio) {
            overlayLines << QStringLiteral("Audio source %1 / %2  |  level %3%")
                .arg(frameTimecode(previewState_.audioSourceFrame,
                                   playbackState_.framesPerSecond))
                .arg(frameTimecode(previewState_.audioSourceDurationFrames,
                                   playbackState_.framesPerSecond))
                .arg(previewState_.audioFadeGainPercent);
        }
    }
    painter.setPen(Qt::white);
    painter.drawText(overlayRect.adjusted(8, 6, -8, -6), Qt::AlignCenter,
                     overlayLines.join(QLatin1Char('\n')));
}

QString QtPreviewPanel::frameTimecode(int frame, int framesPerSecond)
{
    frame = std::max(0, frame);
    framesPerSecond = std::max(1, framesPerSecond);
    const int frames = frame % framesPerSecond;
    const int totalSeconds = frame / framesPerSecond;
    return QStringLiteral("%1:%2:%3:%4")
        .arg(totalSeconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((totalSeconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'))
        .arg(frames, 2, 10, QLatin1Char('0'));
}
