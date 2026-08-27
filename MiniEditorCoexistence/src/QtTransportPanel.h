#pragma once

#include <QWidget>
#include <QString>

#include "ProjectState.h"

class QLabel;
class QToolButton;
class QSlider;

// Qt transport controls. The panel is intentionally unaware of playback
// implementation and works with either preview renderer during migration.
class QtTransportPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit QtTransportPanel(QWidget *parent = nullptr);

    void setPlaybackState(const PlaybackState &state);

signals:
    void playbackCommandRequested(int commandValue);
    void playbackPositionRequested(int frame);

private:
    static QString timecodeText(const PlaybackState &state);

    QToolButton *stepBackwardButton_ = nullptr;
    QToolButton *playPauseButton_ = nullptr;
    QToolButton *stepForwardButton_ = nullptr;
    QToolButton *stopButton_ = nullptr;
    QSlider *positionSlider_ = nullptr;
    QLabel *timecodeLabel_ = nullptr;
};
