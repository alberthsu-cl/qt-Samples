#include "QtEffectSettingsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

namespace {

void addEffect(QComboBox *comboBox, const QString &displayName, EffectType effect)
{
    comboBox->addItem(displayName, static_cast<int>(effect));
}

} // namespace

QtEffectSettingsDialog::QtEffectSettingsDialog(const EffectSettings &currentSettings,
                                               QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Effect Settings (Qt)"));
    setWindowModality(Qt::ApplicationModal);
    setFixedSize(420, 180);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 24, 28, 24);
    mainLayout->setSpacing(20);
    auto *formLayout = new QFormLayout;
    effectCombo_ = new QComboBox(this);

    addEffect(effectCombo_, QStringLiteral("No effect"), EffectType::None);
    addEffect(effectCombo_, QStringLiteral("Grayscale"), EffectType::Grayscale);
    addEffect(effectCombo_, QStringLiteral("Invert"), EffectType::Invert);
    addEffect(effectCombo_, QStringLiteral("Blur"), EffectType::Blur);
    effectCombo_->setCurrentIndex(static_cast<int>(currentSettings.selectedEffect));

    formLayout->addRow(QStringLiteral("Effect:"), effectCombo_);
    mainLayout->addLayout(formLayout);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    // This compact form has a deliberate fixed size, rather than collapsing to
    // the smallest possible width as the Phase 1 MFC dialog did.
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);
}

EffectSettings QtEffectSettingsDialog::selectedSettings() const
{
    EffectSettings settings;
    settings.selectedEffect = static_cast<EffectType>(effectCombo_->currentData().toInt());
    return settings;
}
