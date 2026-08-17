#include "EffectSettingsDialog.h"

IMPLEMENT_DYNAMIC(EffectSettingsDialog, CDialogEx)

EffectSettingsDialog::EffectSettingsDialog(const EffectSettings &currentSettings,
                                           CWnd *parent)
    : CDialogEx(IDD_EFFECT_SETTINGS, parent)
    , selectedSettings_(currentSettings)
{
}

EffectSettings EffectSettingsDialog::selectedSettings() const
{
    return selectedSettings_;
}

void EffectSettingsDialog::DoDataExchange(CDataExchange *dataExchange)
{
    CDialogEx::DoDataExchange(dataExchange);
    DDX_Control(dataExchange, IDC_EFFECT_COMBO, effectCombo_);
}

BOOL EffectSettingsDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Store the enum values in the same order as their underlying integer
    // values. The dialog is merely editing EffectSettings; it owns no effect
    // processing logic.
    effectCombo_.AddString(_T("No effect"));
    effectCombo_.AddString(_T("Grayscale"));
    effectCombo_.AddString(_T("Invert"));
    effectCombo_.AddString(_T("Blur"));
    effectCombo_.SetCurSel(static_cast<int>(selectedSettings_.selectedEffect));

    return TRUE;
}

void EffectSettingsDialog::OnOK()
{
    const int selectedIndex = effectCombo_.GetCurSel();
    if (selectedIndex == CB_ERR) {
        AfxMessageBox(_T("Select an effect before closing this dialog."));
        return;
    }

    selectedSettings_.selectedEffect = static_cast<EffectType>(selectedIndex);
    CDialogEx::OnOK();
}

BEGIN_MESSAGE_MAP(EffectSettingsDialog, CDialogEx)
END_MESSAGE_MAP()
