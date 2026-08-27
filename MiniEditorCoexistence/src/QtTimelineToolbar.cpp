#include "QtTimelineToolbar.h"

#include "TimelineGeometry.h"

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
    , splitButton_(new QToolButton(this))
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
    zoomSlider_->setObjectName(QStringLiteral("timelineZoomSlider"));
    zoomSlider_->setFixedWidth(150);
    zoomLabel_->setMinimumWidth(42);
    fitButton_->setText(QStringLiteral("Fit"));
    fitButton_->setToolTip(QStringLiteral("Reset timeline zoom to 100%"));
    splitButton_->setText(QStringLiteral("Split"));
    splitButton_->setObjectName(QStringLiteral("timelineSplitButton"));
    splitButton_->setToolTip(QStringLiteral(
        "Split the selected clip at the timeline cursor (Ctrl+B)"));
    splitButton_->setEnabled(false);
    rippleButton_->setText(QStringLiteral("Ripple"));
    rippleButton_->setObjectName(QStringLiteral("timelineRippleButton"));
    rippleButton_->setToolTip(QStringLiteral(
        "Shift later clips on the same track during insert, trim, and delete"));
    rippleButton_->setCheckable(true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 6, 10, 6);
    layout->setSpacing(8);
    auto *timelineLabel = new QLabel(QStringLiteral("Timeline"), this);
    timelineLabel->setFixedWidth(TimelineGeometry::kTimelineLeft - layout->spacing());
    timelineLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(timelineLabel);
    layout->addWidget(splitButton_);
    layout->addWidget(rippleButton_);
    layout->addStretch();
    layout->addWidget(fitButton_);
    layout->addWidget(new QLabel(QStringLiteral("Zoom"), this));
    layout->addWidget(zoomSlider_);
    layout->addWidget(zoomLabel_);

    connect(zoomSlider_, &QSlider::valueChanged, this, [this](int zoomPercent) {
        emit viewStateEdited(zoomPercent, isAudioTrackVisible_,
                             rippleButton_->isChecked());
    });
    connect(rippleButton_, &QToolButton::toggled, this, [this](bool isEnabled) {
        emit viewStateEdited(zoomSlider_->value(), isAudioTrackVisible_,
                             isEnabled);
    });
    connect(fitButton_, &QToolButton::clicked, this, &QtTimelineToolbar::fitTimelineRequested);
    connect(splitButton_, &QToolButton::clicked,
            this, &QtTimelineToolbar::splitClipRequested);

    setViewState({});
}

void QtTimelineToolbar::setSplitEnabled(bool isEnabled)
{
    splitButton_->setEnabled(isEnabled);
}

void QtTimelineToolbar::setPresentationState(
    const TimelinePresentationState &state)
{
    setViewState(state.view);
    setSplitEnabled(state.splitEnabled);
}

void QtTimelineToolbar::setViewState(const TimelineViewState &state)
{
    const QSignalBlocker zoomBlocker(zoomSlider_);
    const QSignalBlocker rippleBlocker(rippleButton_);
    zoomSlider_->setValue(state.zoomPercent);
    isAudioTrackVisible_ = state.isAudioTrackVisible;
    rippleButton_->setChecked(state.isRippleEditingEnabled);
    zoomLabel_->setText(QStringLiteral("%1%").arg(state.zoomPercent));
}
