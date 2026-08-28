#pragma once

#include "ProjectState.h"

#include <QWidget>

class QVideoSink;
class QVideoWidget;

// Qt rendering replacement for the learning sample's MFC preview placeholder.
// It consumes framework-neutral presentation data; it does not decode video,
// own playback, or know anything about the MFC host window.
class QtPreviewPanel final : public QWidget
{
public:
    explicit QtPreviewPanel(QWidget *parent = nullptr);

    void setPreviewState(const PreviewState &state);
    void setPlaybackState(const PlaybackState &state);
    QVideoSink *videoSink() const;
    void setDecodedVideoVisible(bool visible);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    static QString frameTimecode(int frame, int framesPerSecond);

    PreviewState previewState_;
    PlaybackState playbackState_;
    QVideoWidget *videoWidget_ = nullptr;
};
