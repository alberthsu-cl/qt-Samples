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
{
    setStyleSheet(QStringLiteral(
        "QtTimelineToolbar { background: #1b1d22; }"
        "QLabel { color: #e6e8ed; }"
        "QToolButton { background: #30343d; color: #e6e8ed; "
        "border: 1px solid #525865; padding: 4px 8px; }"
        "QToolButton:hover { background: #3c5572; }"));

    zoomSlider_->setRange(50, 200);
    zoomSlider_->setFixedWidth(150);
    zoomLabel_->setMinimumWidth(42);
    fitButton_->setText(QStringLiteral("Fit"));
    fitButton_->setToolTip(QStringLiteral("Reset timeline zoom to 100%"));
    audioTrackButton_->setText(QStringLiteral("Audio"));
    audioTrackButton_->setToolTip(QStringLiteral("Show or hide the audio track"));
    audioTrackButton_->setCheckable(true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(8);
    layout->addWidget(new QLabel(QStringLiteral("Timeline"), this));
    layout->addStretch();
    layout->addWidget(audioTrackButton_);
    layout->addWidget(fitButton_);
    layout->addWidget(new QLabel(QStringLiteral("Zoom"), this));
    layout->addWidget(zoomSlider_);
    layout->addWidget(zoomLabel_);

    connect(zoomSlider_, &QSlider::valueChanged, this, [this](int zoomPercent) {
        emit viewStateEdited(zoomPercent, audioTrackButton_->isChecked());
    });
    connect(audioTrackButton_, &QToolButton::toggled, this, [this](bool isVisible) {
        emit viewStateEdited(zoomSlider_->value(), isVisible);
    });
    connect(fitButton_, &QToolButton::clicked, this, &QtTimelineToolbar::fitTimelineRequested);

    setViewState({});
}

void QtTimelineToolbar::setViewState(const TimelineViewState &state)
{
    const QSignalBlocker zoomBlocker(zoomSlider_);
    const QSignalBlocker audioBlocker(audioTrackButton_);
    zoomSlider_->setValue(state.zoomPercent);
    audioTrackButton_->setChecked(state.isAudioTrackVisible);
    zoomLabel_->setText(QStringLiteral("%1%").arg(state.zoomPercent));
}
