#pragma once

#include "EffectSettings.h"
#include "resource.h"

#include <afxwin.h>
#include <afxdlgs.h>
#include <afxdialogex.h>

// Phase 1: an ordinary MFC dialog. In Phase 2, we will replace only this
// presentation layer with a Qt dialog while keeping EffectSettings unchanged.
class EffectSettingsDialog final : public CDialogEx
{
    DECLARE_DYNAMIC(EffectSettingsDialog)

public:
    explicit EffectSettingsDialog(const EffectSettings &currentSettings,
                                  CWnd *parent = nullptr);

    [[nodiscard]] EffectSettings selectedSettings() const;

protected:
    void DoDataExchange(CDataExchange *dataExchange) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    DECLARE_MESSAGE_MAP()

private:
    EffectSettings selectedSettings_;
    CComboBox effectCombo_;
};
