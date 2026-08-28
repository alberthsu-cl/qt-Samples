#pragma once

#include "ProjectState.h"

#include <QImage>
#include <QVideoFrame>
#include <QWidget>

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
    void setStillImage(const QImage &image);
    QVideoSink *videoSink() const;
    void setDecodedVideoVisible(bool visible);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static QString frameTimecode(int frame, int framesPerSecond);

    PreviewState previewState_;
    PlaybackState playbackState_;
    QVideoSink *videoSink_ = nullptr;
    QVideoFrame decodedVideoFrame_;
    QImage stillImage_;
    bool isDecodedVideoVisible_ = false;
};
