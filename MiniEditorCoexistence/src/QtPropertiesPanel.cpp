#include "QtPropertiesPanel.h"

#include "ClipFade.h"

#include <QColor>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QStyleFactory>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QWidget *createSliderEditor(QSlider *slider, QSpinBox *spinBox, QWidget *parent)
{
    auto *editor = new QWidget(parent);
    auto *layout = new QHBoxLayout(editor);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(slider, 1);
    layout->addWidget(spinBox);
    return editor;
}

// QWidget::setStyle() does not take ownership, and the style must outlive
// every widget using it. One process-wide instance is therefore created once
// and never destroyed, which is the usual pattern for a shared QStyle.
QStyle *paletteAwareInputStyle()
{
    static QStyle *style = QStyleFactory::create(QStringLiteral("Fusion"));
    return style;
}

} // namespace

QtPropertiesPanel::QtPropertiesPanel(QWidget *parent)
    : QWidget(parent)
    , opacitySlider_(new QSlider(Qt::Horizontal, this))
    , opacitySpinBox_(new QSpinBox(this))
    , scaleSlider_(new QSlider(Qt::Horizontal, this))
    , scaleSpinBox_(new QSpinBox(this))
    , positionComboBox_(new QComboBox(this))
    , fadeInSlider_(new QSlider(Qt::Horizontal, this))
    , fadeInSpinBox_(new QSpinBox(this))
    , fadeOutSlider_(new QSlider(Qt::Horizontal, this))
    , fadeOutSpinBox_(new QSpinBox(this))
    , fadeSummaryLabel_(new QLabel(this))
{
    setStyleSheet(QStringLiteral(
        "QtPropertiesPanel { background: #23252b; color: #e6e8ed; }"
        "QLabel { color: #e6e8ed; }"
        "QComboBox { background: #31353e; color: #e6e8ed; "
        "border: 1px solid #4a4f5a; padding: 4px; }"));

    // The spin boxes are deliberately left out of the stylesheet above.
    // Styling QSpinBox switches it to the stylesheet style, which then draws
    // its steppers only from ::up-button/::down-button rules and draws no
    // arrow at all unless an image is supplied - that is what made the fade
    // steppers look dead. Fusion honours a palette instead, so these keep the
    // dark look and still get real, clickable up/down buttons.
    QPalette spinBoxPalette = palette();
    spinBoxPalette.setColor(QPalette::Base, QColor(0x31, 0x35, 0x3e));
    spinBoxPalette.setColor(QPalette::Text, QColor(0xe6, 0xe8, 0xed));
    spinBoxPalette.setColor(QPalette::Button, QColor(0x3d, 0x43, 0x4f));
    spinBoxPalette.setColor(QPalette::ButtonText, QColor(0xe6, 0xe8, 0xed));
    spinBoxPalette.setColor(QPalette::Window, QColor(0x31, 0x35, 0x3e));
    spinBoxPalette.setColor(QPalette::WindowText, QColor(0xe6, 0xe8, 0xed));
    for (QSpinBox *spinBox : { opacitySpinBox_, scaleSpinBox_,
                               fadeInSpinBox_, fadeOutSpinBox_ }) {
        if (QStyle *inputStyle = paletteAwareInputStyle())
            spinBox->setStyle(inputStyle);
        spinBox->setPalette(spinBoxPalette);
        // Typing a multi-digit length should be one edit, not one per keystroke.
        spinBox->setKeyboardTracking(false);
    }

    opacitySlider_->setObjectName(QStringLiteral("opacitySlider"));
    opacitySpinBox_->setObjectName(QStringLiteral("opacitySpinBox"));
    scaleSlider_->setObjectName(QStringLiteral("scaleSlider"));
    scaleSpinBox_->setObjectName(QStringLiteral("scaleSpinBox"));
    positionComboBox_->setObjectName(QStringLiteral("positionComboBox"));
    fadeInSlider_->setObjectName(QStringLiteral("fadeInSlider"));
    fadeInSpinBox_->setObjectName(QStringLiteral("fadeInSpinBox"));
    fadeOutSlider_->setObjectName(QStringLiteral("fadeOutSlider"));
    fadeOutSpinBox_->setObjectName(QStringLiteral("fadeOutSpinBox"));
    fadeSummaryLabel_->setObjectName(QStringLiteral("fadeSummaryLabel"));

    opacitySlider_->setRange(0, 100);
    opacitySpinBox_->setRange(0, 100);
    opacitySpinBox_->setSuffix(QStringLiteral(" %"));

    scaleSlider_->setRange(25, 200);
    scaleSpinBox_->setRange(25, 200);
    scaleSpinBox_->setSuffix(QStringLiteral(" %"));

    fadeInSpinBox_->setSuffix(QStringLiteral(" f"));
    fadeOutSpinBox_->setSuffix(QStringLiteral(" f"));
    fadeInSpinBox_->setToolTip(QStringLiteral(
        "Frames the clip takes to ramp up from transparent."));
    fadeOutSpinBox_->setToolTip(QStringLiteral(
        "Frames the clip takes to ramp down to transparent."));
    fadeInSlider_->setToolTip(fadeInSpinBox_->toolTip());
    fadeOutSlider_->setToolTip(fadeOutSpinBox_->toolTip());
    applyFadeRanges();

    positionComboBox_->addItem(QStringLiteral("Center"),
                                static_cast<int>(ClipPosition::Center));
    positionComboBox_->addItem(QStringLiteral("Top Left"),
                                static_cast<int>(ClipPosition::TopLeft));
    positionComboBox_->addItem(QStringLiteral("Top Right"),
                                static_cast<int>(ClipPosition::TopRight));
    positionComboBox_->addItem(QStringLiteral("Bottom Left"),
                                static_cast<int>(ClipPosition::BottomLeft));
    positionComboBox_->addItem(QStringLiteral("Bottom Right"),
                                static_cast<int>(ClipPosition::BottomRight));

    auto *formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->addRow(QStringLiteral("Opacity"),
                       createSliderEditor(opacitySlider_, opacitySpinBox_, this));
    formLayout->addRow(QStringLiteral("Scale"),
                       createSliderEditor(scaleSlider_, scaleSpinBox_, this));
    formLayout->addRow(QStringLiteral("Position"), positionComboBox_);
    formLayout->addRow(QStringLiteral("Fade in"),
                       createSliderEditor(fadeInSlider_, fadeInSpinBox_, this));
    formLayout->addRow(QStringLiteral("Fade out"),
                       createSliderEditor(fadeOutSlider_, fadeOutSpinBox_, this));
    formLayout->addRow(QString(), fadeSummaryLabel_);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->addWidget(new QLabel(QStringLiteral("Properties"), this));
    layout->addLayout(formLayout);
    layout->addStretch();

    // Each slider feeds its spin box. The spin box is the one place that
    // emits the semantic edit, which keeps a slider drag readable.
    connect(opacitySlider_, &QSlider::valueChanged,
            opacitySpinBox_, &QSpinBox::setValue);
    connect(opacitySpinBox_, qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                opacitySlider_->setValue(value);
                emitCurrentSettings();
            });
    connect(scaleSlider_, &QSlider::valueChanged,
            scaleSpinBox_, &QSpinBox::setValue);
    connect(scaleSpinBox_, qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                scaleSlider_->setValue(value);
                emitCurrentSettings();
            });
    connect(positionComboBox_, qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { emitCurrentSettings(); });
    // Each fade uses the same slider/spin-box pairing as opacity and scale,
    // so a ramp can be dragged coarsely and then typed exactly.
    connect(fadeInSlider_, &QSlider::valueChanged,
            fadeInSpinBox_, &QSpinBox::setValue);
    connect(fadeInSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int value) {
                {
                    const QSignalBlocker blockSlider(fadeInSlider_);
                    fadeInSlider_->setValue(value);
                }
                yieldOppositeFade(true);
                updateFadeSummary();
                emitCurrentSettings();
            });
    connect(fadeOutSlider_, &QSlider::valueChanged,
            fadeOutSpinBox_, &QSpinBox::setValue);
    connect(fadeOutSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int value) {
                {
                    const QSignalBlocker blockSlider(fadeOutSlider_);
                    fadeOutSlider_->setValue(value);
                }
                yieldOppositeFade(false);
                updateFadeSummary();
                emitCurrentSettings();
            });
    updateFadeSummary();
}

void QtPropertiesPanel::setClipDurationFrames(int durationFrames)
{
    // A trim can leave the stored pair longer than the clip. Read the current
    // request before the new ranges truncate it, then shorten it with the same
    // proportional rule ClipFade uses, so the panel and the renderer never
    // show different ramps.
    ClipSettings requested;
    requested.fadeInFrames = fadeInSpinBox_->value();
    requested.fadeOutFrames = fadeOutSpinBox_->value();

    clipDurationFrames_ = std::max(0, durationFrames);
    applyFadeRanges();

    const ClipFadeRange fitted = clipDurationFrames_ > 0
        ? ClipFade::normalize(requested, clipDurationFrames_)
        : ClipFadeRange{ requested.fadeInFrames, requested.fadeOutFrames };
    setFadeValues(fitted.fadeInFrames, fitted.fadeOutFrames);
    updateFadeSummary();
}

int QtPropertiesPanel::fadeLimitFrames() const
{
    return clipDurationFrames_ > 0
        ? std::min(clipDurationFrames_, ClipFade::kMaximumFadeFrames)
        : ClipFade::kMaximumFadeFrames;
}

void QtPropertiesPanel::applyFadeRanges()
{
    // Both editors span the whole clip. A range that shrank as the other fade
    // grew was the reason the steppers appeared to stop responding.
    const int limit = fadeLimitFrames();
    const QSignalBlocker blockFadeInSlider(fadeInSlider_);
    const QSignalBlocker blockFadeInSpinBox(fadeInSpinBox_);
    const QSignalBlocker blockFadeOutSlider(fadeOutSlider_);
    const QSignalBlocker blockFadeOutSpinBox(fadeOutSpinBox_);
    fadeInSlider_->setRange(0, limit);
    fadeInSpinBox_->setRange(0, limit);
    fadeOutSlider_->setRange(0, limit);
    fadeOutSpinBox_->setRange(0, limit);
}

void QtPropertiesPanel::yieldOppositeFade(bool fadeInChanged)
{
    if (clipDurationFrames_ <= 0)
        return;

    const int fadeIn = fadeInSpinBox_->value();
    const int fadeOut = fadeOutSpinBox_->value();
    if (fadeIn + fadeOut <= clipDurationFrames_)
        return;

    if (fadeInChanged)
        setFadeValues(fadeIn, clipDurationFrames_ - fadeIn);
    else
        setFadeValues(clipDurationFrames_ - fadeOut, fadeOut);
}

void QtPropertiesPanel::setFadeValues(int fadeInFrames, int fadeOutFrames)
{
    const QSignalBlocker blockFadeInSlider(fadeInSlider_);
    const QSignalBlocker blockFadeInSpinBox(fadeInSpinBox_);
    const QSignalBlocker blockFadeOutSlider(fadeOutSlider_);
    const QSignalBlocker blockFadeOutSpinBox(fadeOutSpinBox_);
    fadeInSlider_->setValue(fadeInFrames);
    fadeInSpinBox_->setValue(fadeInFrames);
    fadeOutSlider_->setValue(fadeOutFrames);
    fadeOutSpinBox_->setValue(fadeOutFrames);
}

void QtPropertiesPanel::updateFadeSummary()
{
    if (clipDurationFrames_ <= 0) {
        fadeSummaryLabel_->setText(QStringLiteral("Select a timeline clip to fade."));
        return;
    }

    const int held = std::max(0, clipDurationFrames_ - fadeInSpinBox_->value()
                                     - fadeOutSpinBox_->value());
    const QString fullLevelText = mediaKind_ == MediaKind::Audio
        ? QStringLiteral("full level") : QStringLiteral("full opacity");
    fadeSummaryLabel_->setText(QStringLiteral("%1 f clip, %2 f at %3")
                                   .arg(clipDurationFrames_)
                                   .arg(held)
                                   .arg(fullLevelText));
}

void QtPropertiesPanel::setViewState(const ClipPropertiesViewState &viewState)
{
    mediaKind_ = viewState.mediaKind;
    setClipDurationFrames(viewState.durationFrames);
    setClipSettings(viewState.settings);
    setEditingEnabled(viewState.editingEnabled);

    const bool isAudio = mediaKind_ == MediaKind::Audio;
    const QString fadeInToolTip = isAudio
        ? QStringLiteral("Frames the clip takes to ramp up from silence.")
        : QStringLiteral("Frames the clip takes to ramp up from transparent.");
    const QString fadeOutToolTip = isAudio
        ? QStringLiteral("Frames the clip takes to ramp down to silence.")
        : QStringLiteral("Frames the clip takes to ramp down to transparent.");
    fadeInSpinBox_->setToolTip(fadeInToolTip);
    fadeInSlider_->setToolTip(fadeInToolTip);
    fadeOutSpinBox_->setToolTip(fadeOutToolTip);
    fadeOutSlider_->setToolTip(fadeOutToolTip);
    updateFadeSummary();
}

void QtPropertiesPanel::setClipSettings(const ClipSettings &settings)
{
    // MFC is applying the stored model state. Suppress each widget's signal so
    // selection changes never look like user edits returning to MFC.
    const QSignalBlocker blockOpacitySlider(opacitySlider_);
    const QSignalBlocker blockOpacitySpinBox(opacitySpinBox_);
    const QSignalBlocker blockScaleSlider(scaleSlider_);
    const QSignalBlocker blockScaleSpinBox(scaleSpinBox_);
    const QSignalBlocker blockPosition(positionComboBox_);
    const QSignalBlocker blockFadeInSlider(fadeInSlider_);
    const QSignalBlocker blockFadeInSpinBox(fadeInSpinBox_);
    const QSignalBlocker blockFadeOutSlider(fadeOutSlider_);
    const QSignalBlocker blockFadeOutSpinBox(fadeOutSpinBox_);
    opacitySlider_->setValue(settings.opacityPercent);
    opacitySpinBox_->setValue(settings.opacityPercent);
    scaleSlider_->setValue(settings.scalePercent);
    scaleSpinBox_->setValue(settings.scalePercent);
    positionComboBox_->setCurrentIndex(positionComboBox_->findData(
        static_cast<int>(settings.position)));
    setFadeValues(settings.fadeInFrames, settings.fadeOutFrames);
    updateFadeSummary();
}

void QtPropertiesPanel::setEditingEnabled(bool enabled)
{
    opacitySlider_->setEnabled(enabled);
    opacitySpinBox_->setEnabled(enabled);
    scaleSlider_->setEnabled(enabled);
    scaleSpinBox_->setEnabled(enabled);
    positionComboBox_->setEnabled(enabled);
    fadeInSlider_->setEnabled(enabled);
    fadeInSpinBox_->setEnabled(enabled);
    fadeOutSlider_->setEnabled(enabled);
    fadeOutSpinBox_->setEnabled(enabled);
    setToolTip(enabled
        ? QString()
        : QStringLiteral("Select a timeline clip to edit placement properties."));
}

void QtPropertiesPanel::emitCurrentSettings()
{
    emit clipSettingsEdited(opacitySpinBox_->value(),
                            scaleSpinBox_->value(),
                            positionComboBox_->currentData().toInt(),
                            fadeInSpinBox_->value(),
                            fadeOutSpinBox_->value());
}
