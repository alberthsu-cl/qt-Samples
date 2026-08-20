#include "QtTransportPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QToolButton>

namespace {

QToolButton *createTransportButton(const QString &text, const QString &toolTip, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(toolTip);
    button->setFixedSize(54, 30);
    return button;
}

} // namespace

QtTransportPanel::QtTransportPanel(QWidget *parent)
    : QWidget(parent)
    , stepBackwardButton_(createTransportButton(QStringLiteral("<"),
                                                QStringLiteral("Previous frame"), this))
    , playPauseButton_(createTransportButton(QStringLiteral("Play"),
                                             QStringLiteral("Play or pause"), this))
    , stepForwardButton_(createTransportButton(QStringLiteral(">"),
                                               QStringLiteral("Next frame"), this))
    , stopButton_(createTransportButton(QStringLiteral("Stop"),
                                        QStringLiteral("Stop playback"), this))
    , timecodeLabel_(new QLabel(this))
{
    setStyleSheet(QStringLiteral(
        "QtTransportPanel { background: #1b1d22; }"
        "QToolButton { background: #30343d; color: #e6e8ed; "
        "border: 1px solid #525865; padding: 3px; }"
        "QToolButton:hover { background: #3c5572; }"
        "QLabel { background: #101114; color: #e6e8ed; padding: 6px; }"));

    playPauseButton_->setCheckable(true);
    timecodeLabel_->setMinimumWidth(130);
    timecodeLabel_->setAlignment(Qt::AlignCenter);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(8);
    layout->addWidget(stepBackwardButton_);
    layout->addWidget(playPauseButton_);
    layout->addWidget(stepForwardButton_);
    layout->addWidget(stopButton_);
    layout->addStretch();
    layout->addWidget(timecodeLabel_);

    connect(stepBackwardButton_, &QToolButton::clicked, this,
            [this] { emit playbackCommandRequested(static_cast<int>(PlaybackCommand::StepBackward)); });
    connect(playPauseButton_, &QToolButton::clicked, this,
            [this] { emit playbackCommandRequested(static_cast<int>(PlaybackCommand::TogglePlayPause)); });
    connect(stepForwardButton_, &QToolButton::clicked, this,
            [this] { emit playbackCommandRequested(static_cast<int>(PlaybackCommand::StepForward)); });
    connect(stopButton_, &QToolButton::clicked, this,
            [this] { emit playbackCommandRequested(static_cast<int>(PlaybackCommand::Stop)); });

    setPlaybackState({});
}

void QtTransportPanel::setPlaybackState(const PlaybackState &state)
{
    // MFC synchronizes the true playback state. Blocking clicked/toggled keeps
    // this view update from becoming a false command back to the MFC owner.
    const QSignalBlocker signalBlocker(playPauseButton_);
    playPauseButton_->setChecked(state.isPlaying);
    playPauseButton_->setText(state.isPlaying ? QStringLiteral("Pause")
                                               : QStringLiteral("Play"));
    timecodeLabel_->setText(timecodeText(state));
}

QString QtTransportPanel::timecodeText(const PlaybackState &state)
{
    const int frames = state.currentFrame % state.framesPerSecond;
    const int totalSeconds = state.currentFrame / state.framesPerSecond;
    const int seconds = totalSeconds % 60;
    const int minutes = (totalSeconds / 60) % 60;
    const int hours = totalSeconds / 3600;
    return QStringLiteral("%1:%2:%3:%4")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(frames, 2, 10, QLatin1Char('0'));
}
