#pragma once

#include "ProjectState.h"

#include <QImage>
#include <QVideoFrame>
#include <QWidget>

#include <functional>

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

    // M5-05, decision B: the new engine gets its own presentation surface
    // inside this panel. It is a *second* sink, not a redirection of the one
    // above -- source preview keeps driving that one unchanged, so the two
    // paths never contend for a sink and neither can silently start rendering
    // the other's frames.
    QVideoSink *engineVideoSink() const;

    // While active, the panel paints what arrives on the engine sink instead
    // of the legacy source-preview content.
    void setEnginePresentationActive(bool active);

    // Invoked on the GUI thread once per frame, immediately after that frame
    // has been committed to this surface -- ADR-003's FramePresented moment.
    // The panel deliberately knows no presentation identity: whoever owns the
    // coordinator attaches it.
    void setEngineFrameCommittedHandler(std::function<void()> handler);

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
    QVideoSink *engineVideoSink_ = nullptr;
    QVideoFrame decodedVideoFrame_;
    QVideoFrame engineVideoFrame_;
    QImage engineImage_;
    bool isEnginePresentationActive_ = false;
    // False from the moment a new engine frame arrives until it has actually
    // been painted, so one acknowledgement is emitted per frame rather than
    // per repaint.
    bool hasAcknowledgedEngineFrame_ = true;
    std::function<void()> engineFrameCommittedHandler_;
    QImage fallbackImage_;
    bool isDecodedVideoVisible_ = false;
    QtPreviewEffectPipeline *effectPipeline_ = nullptr;
    QImage processedImage_;
};
