#pragma once

#include <QWidget>

#include "ProjectState.h"

class QComboBox;
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

    void setClipSettings(const ClipSettings &settings);
    void setEditingEnabled(bool enabled);

signals:
    void clipSettingsEdited(int opacityPercent, int scalePercent, int positionValue);

private:
    void emitCurrentSettings();

    QSlider *opacitySlider_ = nullptr;
    QSpinBox *opacitySpinBox_ = nullptr;
    QSlider *scaleSlider_ = nullptr;
    QSpinBox *scaleSpinBox_ = nullptr;
    QComboBox *positionComboBox_ = nullptr;
};
