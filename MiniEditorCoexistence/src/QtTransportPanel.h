#pragma once

#include <QWidget>
#include <QString>

#include "ProjectState.h"

class QLabel;
class QToolButton;

// Qt replacement for only the transport controls under an existing MFC
// preview surface. It is intentionally unaware of playback implementation.
class QtTransportPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit QtTransportPanel(QWidget *parent = nullptr);

    void setPlaybackState(const PlaybackState &state);

signals:
    void playbackCommandRequested(int commandValue);

private:
    static QString timecodeText(const PlaybackState &state);

    QToolButton *stepBackwardButton_ = nullptr;
    QToolButton *playPauseButton_ = nullptr;
    QToolButton *stepForwardButton_ = nullptr;
    QToolButton *stopButton_ = nullptr;
    QLabel *timecodeLabel_ = nullptr;
};
