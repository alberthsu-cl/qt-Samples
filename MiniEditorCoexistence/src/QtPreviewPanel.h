#pragma once

#include "ProjectState.h"

#include <QImage>
#include <QVideoFrame>
#include <QWidget>

class QtPreviewEffectPipeline;
class QVideoSink;

// Qt rendering replacement for the learning sample's MFC preview placeholder.
// It consumes framework-neutral presentation data; it does not decode video,
// own playback, or know anything about the MFC host window.
class QtPreviewPanel final : public QWidget
{
public:
    explicit QtPreviewPanel(QWidget *parent = nullptr);

    void setPreviewState(const PreviewState &state);
    void setPlaybackState(const PlaybackState &state);
    // Cached source thumbnail used for still images and while a newly selected
    // video is waiting for its first decoded frame.
    void setFallbackImage(const QImage &image);
    QVideoSink *videoSink() const;
    void setDecodedVideoVisible(bool visible);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static QString frameTimecode(int frame, int framesPerSecond);

    // Hands the newest source frame to the worker thread. Painting keeps
    // using the previous processed frame until a new result arrives, so the
    // preview never blocks on the effect.
    void submitFrameForProcessing(const QImage &frame);
    QImage sourceImageToPaint() const;

    PreviewState previewState_;
    PlaybackState playbackState_;
    QVideoSink *videoSink_ = nullptr;
    QVideoFrame decodedVideoFrame_;
    QImage fallbackImage_;
    bool isDecodedVideoVisible_ = false;
    QtPreviewEffectPipeline *effectPipeline_ = nullptr;
    QImage processedImage_;
};
