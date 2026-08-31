#pragma once

#include <QWidget>

#include "ClipEffect.h"
#include "ClipPropertiesStateResolver.h"

class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QSlider;
class QSpinBox;

// Phase 2 replacement for MfcPropertiesPane. It presents settings but does not
// own them; MFC remains the owner of the selected clip's project state.
class QtPropertiesPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit QtPropertiesPanel(QWidget *parent = nullptr);

    void setViewState(const ClipPropertiesViewState &viewState);
    // Kept public as focused teaching/test seams. Production refreshes use
    // setViewState() so duration, values, media kind, and enabled state agree.
    void setClipSettings(const ClipSettings &settings);
    // The placement length bounds both fade editors, so the panel can never
    // request a ramp that ClipFade would silently shorten again.
    void setClipDurationFrames(int durationFrames);
    void setEditingEnabled(bool enabled);

signals:
    void clipSettingsEdited(int opacityPercent, int scalePercent, int positionValue,
                            int fadeInFrames, int fadeOutFrames,
                            int effectValue, int effectIntensityPercent);

private:
    void emitCurrentSettings();

    QSlider *opacitySlider_ = nullptr;
    QSpinBox *opacitySpinBox_ = nullptr;
    QSlider *scaleSlider_ = nullptr;
    QSpinBox *scaleSpinBox_ = nullptr;
    QComboBox *positionComboBox_ = nullptr;
    QFormLayout *sizePositionLayout_ = nullptr;
    QFormLayout *opacityFadingLayout_ = nullptr;
    QGroupBox *sizePositionGroup_ = nullptr;
    QGroupBox *opacityFadingGroup_ = nullptr;
    QGroupBox *dspGroup_ = nullptr;
    QComboBox *effectComboBox_ = nullptr;
    QSlider *effectIntensitySlider_ = nullptr;
    QSpinBox *effectIntensitySpinBox_ = nullptr;
    QWidget *effectIntensityEditor_ = nullptr;
    QWidget *opacityEditor_ = nullptr;
    QWidget *scaleEditor_ = nullptr;
    QSlider *fadeInSlider_ = nullptr;
    QSpinBox *fadeInSpinBox_ = nullptr;
    QSlider *fadeOutSlider_ = nullptr;
    QSpinBox *fadeOutSpinBox_ = nullptr;
    QLabel *fadeSummaryLabel_ = nullptr;
    QLabel *selectionMessageLabel_ = nullptr;
    QWidget *formContainer_ = nullptr;
    int clipDurationFrames_ = 0;
    MediaKind mediaKind_ = MediaKind::Video;

    int fadeLimitFrames() const;
    void applyFadeRanges();
    // The fade being dragged always moves freely across its whole range; the
    // other one yields so the pair still fits inside the clip.
    void yieldOppositeFade(bool fadeInChanged);
    void setFadeValues(int fadeInFrames, int fadeOutFrames);
    void updateTargetPresentation(const ClipPropertiesViewState &viewState);
    void updateMediaSpecificRows();
    void updateFadeSummary();
    void updateEffectIntensityEnabled();
};
