#include "QtPropertiesPanel.h"

#include "ClipFade.h"
#include "FrameTimecode.h"

#include <QColor>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
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

QString frameCountText(int frameCount)
{
    const int nonNegativeFrameCount = std::max(0, frameCount);
    const QString unit = nonNegativeFrameCount == 1
        ? QStringLiteral("frame") : QStringLiteral("frames");
    return QStringLiteral("%1 %2").arg(nonNegativeFrameCount).arg(unit);
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
    , selectionMessageLabel_(new QLabel(this))
    , formContainer_(new QWidget(this))
{
    setStyleSheet(QStringLiteral(
        "QtPropertiesPanel { background: #23252b; color: #e6e8ed; }"
        "QLabel { color: #e6e8ed; }"
        "QGroupBox { color: #cfd4de; font-weight: 600; "
        "border: 1px solid #3b3f48; margin-top: 10px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 9px; padding: 0 4px; }"
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
    fadeSummaryLabel_->setAlignment(Qt::AlignHCenter);
    selectionMessageLabel_->setObjectName(QStringLiteral("selectionMessageLabel"));
    selectionMessageLabel_->setWordWrap(true);
    formContainer_->setObjectName(QStringLiteral("propertiesFormContainer"));

    opacitySlider_->setRange(0, 100);
    opacitySpinBox_->setRange(0, 100);
    opacitySpinBox_->setSuffix(QStringLiteral(" %"));

    scaleSlider_->setRange(25, 200);
    scaleSpinBox_->setRange(25, 200);
    scaleSpinBox_->setSuffix(QStringLiteral(" %"));

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

    sizePositionGroup_ = new QGroupBox(QStringLiteral("Size / Position"), formContainer_);
    sizePositionGroup_->setObjectName(QStringLiteral("sizePositionGroup"));
    sizePositionLayout_ = new QFormLayout(sizePositionGroup_);
    sizePositionLayout_->setLabelAlignment(Qt::AlignRight);

    opacityFadingGroup_ = new QGroupBox(QStringLiteral("Opacity / Fading"), formContainer_);
    opacityFadingGroup_->setObjectName(QStringLiteral("opacityFadingGroup"));
    opacityFadingLayout_ = new QFormLayout(opacityFadingGroup_);
    opacityFadingLayout_->setLabelAlignment(Qt::AlignRight);

    dspGroup_ = new QGroupBox(QStringLiteral("DSP"), formContainer_);
    dspGroup_->setObjectName(QStringLiteral("dspGroup"));
    auto *dspLayout = new QFormLayout(dspGroup_);
    dspLayout->setLabelAlignment(Qt::AlignRight);

    effectComboBox_ = new QComboBox(dspGroup_);
    effectComboBox_->setObjectName(QStringLiteral("effectComboBox"));
    for (const ClipEffectKind effect : { ClipEffectKind::None,
                                         ClipEffectKind::Grayscale,
                                         ClipEffectKind::Invert,
                                         ClipEffectKind::Blur }) {
        effectComboBox_->addItem(
            QString::fromWCharArray(clipEffectDisplayName(effect)),
            static_cast<int>(effect));
    }

    effectIntensitySlider_ = new QSlider(Qt::Horizontal, dspGroup_);
    effectIntensitySlider_->setObjectName(QStringLiteral("effectIntensitySlider"));
    effectIntensitySpinBox_ = new QSpinBox(dspGroup_);
    effectIntensitySpinBox_->setObjectName(QStringLiteral("effectIntensitySpinBox"));
    effectIntensitySpinBox_->setSuffix(QStringLiteral(" %"));
    effectIntensitySpinBox_->setKeyboardTracking(false);
    if (QStyle *inputStyle = paletteAwareInputStyle())
        effectIntensitySpinBox_->setStyle(inputStyle);
    effectIntensitySlider_->setRange(kMinimumEffectIntensityPercent,
                                     kMaximumEffectIntensityPercent);
    effectIntensitySpinBox_->setRange(kMinimumEffectIntensityPercent,
                                      kMaximumEffectIntensityPercent);
    effectIntensityEditor_ = createSliderEditor(effectIntensitySlider_,
                                                effectIntensitySpinBox_, dspGroup_);
    effectIntensityEditor_->setObjectName(QStringLiteral("effectIntensityEditor"));

    dspLayout->addRow(QStringLiteral("Effect"), effectComboBox_);
    dspLayout->addRow(QStringLiteral("Intensity"), effectIntensityEditor_);
    auto *dspNote = new QLabel(
        QStringLiteral("Apply on selected timeline clip."), dspGroup_);
    dspNote->setObjectName(QStringLiteral("dspNoteLabel"));
    dspNote->setWordWrap(true);
    dspNote->setStyleSheet(QStringLiteral("color: #9da3af; font-weight: normal;"));
    dspLayout->addRow(dspNote);

    opacityEditor_ = createSliderEditor(opacitySlider_, opacitySpinBox_, this);
    opacityEditor_->setObjectName(QStringLiteral("opacityEditor"));
    scaleEditor_ = createSliderEditor(scaleSlider_, scaleSpinBox_, this);
    scaleEditor_->setObjectName(QStringLiteral("scaleEditor"));
    sizePositionLayout_->addRow(QStringLiteral("Scale"), scaleEditor_);
    sizePositionLayout_->addRow(QStringLiteral("Position"), positionComboBox_);
    opacityFadingLayout_->addRow(QStringLiteral("Opacity"), opacityEditor_);
    opacityFadingLayout_->addRow(QStringLiteral("Fade in"),
                                 createSliderEditor(fadeInSlider_, fadeInSpinBox_, this));
    opacityFadingLayout_->addRow(QStringLiteral("Fade out"),
                                 createSliderEditor(fadeOutSlider_, fadeOutSpinBox_, this));
    // A one-widget QFormLayout row spans both columns. This keeps the summary
    // aligned with the left edge of the captions instead of the input fields.
    opacityFadingLayout_->addRow(fadeSummaryLabel_);

    auto *formSectionsLayout = new QVBoxLayout(formContainer_);
    formSectionsLayout->setContentsMargins(0, 0, 0, 0);
    formSectionsLayout->setSpacing(8);
    formSectionsLayout->addWidget(sizePositionGroup_);
    formSectionsLayout->addWidget(opacityFadingGroup_);
    formSectionsLayout->addWidget(dspGroup_);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->addWidget(new QLabel(QStringLiteral("Properties"), this));
    layout->addWidget(selectionMessageLabel_);
    layout->addWidget(formContainer_);
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
    connect(effectComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) {
                updateEffectIntensityEnabled();
                emitCurrentSettings();
            });
    connect(effectIntensitySlider_, &QSlider::valueChanged,
            effectIntensitySpinBox_, &QSpinBox::setValue);
    connect(effectIntensitySpinBox_, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int value) {
                const QSignalBlocker blockSlider(effectIntensitySlider_);
                effectIntensitySlider_->setValue(value);
                emitCurrentSettings();
            });

    updateFadeSummary();
    updateEffectIntensityEnabled();
    updateTargetPresentation({});
}

void QtPropertiesPanel::updateEffectIntensityEnabled()
{
    const bool hasEffect = effectComboBox_->currentData().toInt()
        != static_cast<int>(ClipEffectKind::None);
    effectIntensityEditor_->setEnabled(hasEffect);
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
    fadeSummaryLabel_->setText(QStringLiteral("%1 of %2 at %3")
                                   .arg(held)
                                   .arg(frameCountText(clipDurationFrames_))
                                   .arg(fullLevelText));
}

void QtPropertiesPanel::updateMediaSpecificRows()
{
    const bool hasVisualPlacement = mediaKind_ != MediaKind::Audio;
    sizePositionGroup_->setVisible(hasVisualPlacement);
    opacityFadingLayout_->setRowVisible(opacityEditor_, hasVisualPlacement);
    dspGroup_->setVisible(hasVisualPlacement);
}

void QtPropertiesPanel::updateTargetPresentation(
    const ClipPropertiesViewState &viewState)
{
    const ClipPropertiesTarget target = viewState.target;
    const bool isTimelineClip = target == ClipPropertiesTarget::TimelineClip;
    formContainer_->setVisible(isTimelineClip);
    selectionMessageLabel_->setVisible(!isTimelineClip);

    if (target == ClipPropertiesTarget::MediaAsset) {
        const QString displayName = viewState.mediaDisplayName.empty()
            ? QStringLiteral("Unknown media")
            : QString::fromStdWString(viewState.mediaDisplayName);
        const QString type = viewState.mediaKind == MediaKind::Video
            ? QStringLiteral("Video")
            : viewState.mediaKind == MediaKind::Audio
                ? QStringLiteral("Audio") : QStringLiteral("Image");
        const QString sourcePath = viewState.mediaFilePath.empty()
            ? QStringLiteral("Unavailable")
            : QString::fromStdWString(viewState.mediaFilePath);
        const QString duration = QString::fromStdWString(
            frameTimecodeMmSsFf(viewState.durationFrames));
        selectionMessageLabel_->setText(QStringLiteral(
            "Name: %1\nType: %2\nDuration: %3\nSource: %4\n\n"
            "Add this media to the timeline to edit placement properties.")
            .arg(displayName, type)
            .arg(duration)
            .arg(sourcePath));
    } else if (target == ClipPropertiesTarget::EmptyTimeline) {
        selectionMessageLabel_->setText(QStringLiteral(
            "Select a timeline clip to edit its properties."));
    } else {
        selectionMessageLabel_->setText(QStringLiteral(
            "Select media or a timeline clip to view its properties."));
    }
}

void QtPropertiesPanel::setViewState(const ClipPropertiesViewState &viewState)
{
    mediaKind_ = viewState.mediaKind;
    updateTargetPresentation(viewState);
    updateMediaSpecificRows();
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
    const QSignalBlocker blockEffect(effectComboBox_);
    const QSignalBlocker blockEffectSlider(effectIntensitySlider_);
    const QSignalBlocker blockEffectSpinBox(effectIntensitySpinBox_);
    opacitySlider_->setValue(settings.opacityPercent);
    opacitySpinBox_->setValue(settings.opacityPercent);
    scaleSlider_->setValue(settings.scalePercent);
    scaleSpinBox_->setValue(settings.scalePercent);
    positionComboBox_->setCurrentIndex(positionComboBox_->findData(
        static_cast<int>(settings.position)));
    setFadeValues(settings.fadeInFrames, settings.fadeOutFrames);
    const int effectIndex = effectComboBox_->findData(
        static_cast<int>(settings.effect));
    if (effectIndex >= 0)
        effectComboBox_->setCurrentIndex(effectIndex);
    effectIntensitySlider_->setValue(settings.effectIntensityPercent);
    effectIntensitySpinBox_->setValue(settings.effectIntensityPercent);
    updateEffectIntensityEnabled();
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
    effectComboBox_->setEnabled(enabled);
    effectIntensityEditor_->setEnabled(enabled
        && effectComboBox_->currentData().toInt()
            != static_cast<int>(ClipEffectKind::None));
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
                            fadeOutSpinBox_->value(),
                            effectComboBox_->currentData().toInt(),
                            effectIntensitySpinBox_->value());
}
