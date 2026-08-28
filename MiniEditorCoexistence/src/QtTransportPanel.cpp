#include "QtTransportPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QToolButton>

#include <algorithm>

namespace {

class ClickSeekSlider final : public QSlider
{
public:
    using QSlider::QSlider;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            QSlider::mousePressEvent(event);
            return;
        }

        QStyleOptionSlider option;
        initStyleOption(&option);
        const QRect handle = style()->subControlRect(
            QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
        if (handle.contains(event->position().toPoint())) {
            QSlider::mousePressEvent(event);
            return;
        }

        const int sliderLength = handle.width();
        const int availableLength = std::max(1, width() - sliderLength);
        const int clickPosition = std::clamp(
            qRound(event->position().x()) - sliderLength / 2,
            0, availableLength);
        const int clickedValue = QStyle::sliderValueFromPosition(
            minimum(), maximum(), clickPosition, availableLength,
            option.upsideDown);
        setValue(clickedValue);
        emit sliderMoved(clickedValue);
        event->accept();
    }
};

QToolButton *createTransportButton(const QString &text, const QString &toolTip,
                                   const QSize &size, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(toolTip);
    button->setFixedSize(size);
    return button;
}

} // namespace

QtTransportPanel::QtTransportPanel(QWidget *parent)
    : QWidget(parent)
    , stepBackwardButton_(createTransportButton(QStringLiteral("<"),
                                                QStringLiteral("Previous frame"),
                                                QSize(54, 30), this))
    , playPauseButton_(createTransportButton(QStringLiteral("Play"),
                                             QStringLiteral("Play playback"),
                                             QSize(54, 30), this))
    , stepForwardButton_(createTransportButton(QStringLiteral(">"),
                                               QStringLiteral("Next frame"),
                                               QSize(54, 30), this))
    , stopButton_(createTransportButton(QStringLiteral("Stop"),
                                        QStringLiteral("Stop playback"),
                                        QSize(54, 30), this))
    , positionSlider_(new ClickSeekSlider(Qt::Horizontal, this))
    , timecodeLabel_(new QLabel(this))
{
    stepBackwardButton_->setObjectName(QStringLiteral("stepBackwardButton"));
    playPauseButton_->setObjectName(QStringLiteral("playPauseButton"));
    stepForwardButton_->setObjectName(QStringLiteral("stepForwardButton"));
    stopButton_->setObjectName(QStringLiteral("stopButton"));
    positionSlider_->setObjectName(QStringLiteral("positionSlider"));
    timecodeLabel_->setObjectName(QStringLiteral("timecodeLabel"));

    setStyleSheet(QStringLiteral(
        "QtTransportPanel { background: #1b1d22; }"
        // Keep the original, text-labelled controls together in one simple
        // rectangular group.  Square corners avoid the clipped-looking arcs
        // that were visible at the edge of the native MFC host rectangle.
        "QWidget#transportControls { background: #30343d; "
        "border: 1px solid #525865; border-radius: 0; }"
        "QToolButton { background: transparent; color: #e6e8ed; "
        "border: none; border-right: 1px solid #525865; padding: 3px; }"
        "QToolButton#stopButton { border-right: none; }"
        "QToolButton:hover { background: #3c5572; }"
        "QToolButton#playPauseButton { background: #2f7ed8; "
        "border-right: 1px solid #5aa8ff; }"
        "QToolButton#playPauseButton:hover { background: #4796ec; }"
        "QToolButton#playPauseButton:checked { background: #b96b2c; "
        "border-color: #e7a45d; }"
        "QLabel { background: #101114; color: #e6e8ed; padding: 6px; "
        "border: 1px solid #30343d; border-radius: 3px; "
        "font-family: Consolas, monospace; }"));

    playPauseButton_->setCheckable(true);
    positionSlider_->setMinimumWidth(80);
    // The native Windows slider positions its groove correctly at its natural
    // height. Keep that height and center the whole slider beside the buttons.
    positionSlider_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    timecodeLabel_->setMinimumWidth(130);
    timecodeLabel_->setAlignment(Qt::AlignCenter);

    auto *transportControls = new QWidget(this);
    transportControls->setObjectName(QStringLiteral("transportControls"));
    auto *controlsLayout = new QHBoxLayout(transportControls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(0);
    controlsLayout->addWidget(stepBackwardButton_);
    controlsLayout->addWidget(playPauseButton_);
    controlsLayout->addWidget(stepForwardButton_);
    controlsLayout->addWidget(stopButton_);

    auto *layout = new QHBoxLayout(this);
    // This panel is hosted in an MFC-managed rectangle that can become as
    // narrow as the preview's minimum width. Do not let fixed transport
    // children force a larger QWidget and get clipped by that native host.
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(8);
    layout->addWidget(transportControls);
    // Use Qt's native slider appearance, exactly like the Zoom control. It
    // supplies matching minimum/maximum endpoint spacing by itself.
    layout->addWidget(positionSlider_, 1, Qt::AlignVCenter);
    layout->addWidget(timecodeLabel_);

    connect(stepBackwardButton_, &QToolButton::clicked, this,
            [this] { emit playbackCommandRequested(static_cast<int>(PlaybackCommand::StepBackward)); });
    connect(playPauseButton_, &QToolButton::clicked, this,
            [this] { emit playbackCommandRequested(static_cast<int>(PlaybackCommand::TogglePlayPause)); });
    connect(stepForwardButton_, &QToolButton::clicked, this,
            [this] { emit playbackCommandRequested(static_cast<int>(PlaybackCommand::StepForward)); });
    connect(stopButton_, &QToolButton::clicked, this,
            [this] { emit playbackCommandRequested(static_cast<int>(PlaybackCommand::Stop)); });
    connect(positionSlider_, &QSlider::sliderMoved, this,
            &QtTransportPanel::playbackPositionRequested);

    setPlaybackState({});
    updateResponsiveControls();
}

void QtTransportPanel::setPlaybackState(const PlaybackState &state)
{
    // MFC synchronizes the true playback state. Blocking clicked/toggled keeps
    // this view update from becoming a false command back to the MFC owner.
    const QSignalBlocker signalBlocker(playPauseButton_);
    const QSignalBlocker sliderBlocker(positionSlider_);
    playPauseButton_->setChecked(state.isPlaying);
    playPauseButton_->setText(state.isPlaying ? QStringLiteral("Pause")
                                               : QStringLiteral("Play"));
    playPauseButton_->setToolTip(state.isPlaying ? QStringLiteral("Pause playback")
                                                  : QStringLiteral("Play playback"));
    positionSlider_->setRange(0, std::max(0, state.durationFrames - 1));
    positionSlider_->setValue(state.currentFrame);
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

void QtTransportPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateResponsiveControls();
}

void QtTransportPanel::updateResponsiveControls()
{
    // A minimum preview width can be much narrower than the complete
    // transport. Preserve a useful seek bar first, then reveal secondary
    // controls as the panel receives more room.
    const bool showFrameSteps = width() >= 560;
    const bool showTimecode = width() >= 420;
    stepBackwardButton_->setVisible(showFrameSteps);
    stepForwardButton_->setVisible(showFrameSteps);
    timecodeLabel_->setVisible(showTimecode);
}
