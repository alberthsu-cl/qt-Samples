#include "QtTimelineToolbar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>

QtTimelineToolbar::QtTimelineToolbar(QWidget *parent)
    : QWidget(parent)
    , zoomSlider_(new QSlider(Qt::Horizontal, this))
    , zoomLabel_(new QLabel(this))
    , fitButton_(new QToolButton(this))
    , audioTrackButton_(new QToolButton(this))
    , rippleButton_(new QToolButton(this))
{
    setStyleSheet(QStringLiteral(
        "QtTimelineToolbar { background: #1b1d22; }"
        "QLabel { color: #e6e8ed; }"
        "QToolButton { background: #30343d; color: #e6e8ed; "
        "border: 1px solid #525865; padding: 4px 8px; }"
        "QToolButton:hover { background: #3c5572; }"
        "QToolButton:checked { background: #2a88eb; border-color: #69adf5; }"));

    zoomSlider_->setRange(50, 200);
    zoomSlider_->setFixedWidth(150);
    zoomLabel_->setMinimumWidth(42);
    fitButton_->setText(QStringLiteral("Fit"));
    fitButton_->setToolTip(QStringLiteral("Reset timeline zoom to 100%"));
    audioTrackButton_->setText(QStringLiteral("Audio"));
    audioTrackButton_->setToolTip(QStringLiteral("Show or hide the audio track"));
    audioTrackButton_->setCheckable(true);
    rippleButton_->setText(QStringLiteral("Ripple"));
    rippleButton_->setToolTip(QStringLiteral(
        "Shift later clips on the same track during insert, trim, and delete"));
    rippleButton_->setCheckable(true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(8);
    layout->addWidget(new QLabel(QStringLiteral("Timeline"), this));
    layout->addStretch();
    layout->addWidget(rippleButton_);
    layout->addWidget(audioTrackButton_);
    layout->addWidget(fitButton_);
    layout->addWidget(new QLabel(QStringLiteral("Zoom"), this));
    layout->addWidget(zoomSlider_);
    layout->addWidget(zoomLabel_);

    connect(zoomSlider_, &QSlider::valueChanged, this, [this](int zoomPercent) {
        emit viewStateEdited(zoomPercent, audioTrackButton_->isChecked(),
                             rippleButton_->isChecked());
    });
    connect(audioTrackButton_, &QToolButton::toggled, this, [this](bool isVisible) {
        emit viewStateEdited(zoomSlider_->value(), isVisible,
                             rippleButton_->isChecked());
    });
    connect(rippleButton_, &QToolButton::toggled, this, [this](bool isEnabled) {
        emit viewStateEdited(zoomSlider_->value(), audioTrackButton_->isChecked(),
                             isEnabled);
    });
    connect(fitButton_, &QToolButton::clicked, this, &QtTimelineToolbar::fitTimelineRequested);

    setViewState({});
}

void QtTimelineToolbar::setViewState(const TimelineViewState &state)
{
    const QSignalBlocker zoomBlocker(zoomSlider_);
    const QSignalBlocker audioBlocker(audioTrackButton_);
    const QSignalBlocker rippleBlocker(rippleButton_);
    zoomSlider_->setValue(state.zoomPercent);
    audioTrackButton_->setChecked(state.isAudioTrackVisible);
    rippleButton_->setChecked(state.isRippleEditingEnabled);
    zoomLabel_->setText(QStringLiteral("%1%").arg(state.zoomPercent));
}
