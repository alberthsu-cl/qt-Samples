#pragma once

#include "EffectSettings.h"

#include <QDialog>

class QComboBox;

// Phase 2 replacement for the MFC CDialogEx implementation. It accepts and
// returns the same framework-neutral EffectSettings value as the MFC dialog.
class QtEffectSettingsDialog final : public QDialog
{
public:
    explicit QtEffectSettingsDialog(const EffectSettings &currentSettings,
                                    QWidget *parent = nullptr);

    [[nodiscard]] EffectSettings selectedSettings() const;

private:
    QComboBox *effectCombo_ = nullptr;
};
