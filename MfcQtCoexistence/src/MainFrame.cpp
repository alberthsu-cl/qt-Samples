#include "MainFrame.h"

#include "EffectSettingsDialog.h"
#include "resource.h"

IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)

namespace {

constexpr UINT kStatusBarIndicators[] = { ID_SEPARATOR };

} // namespace

int MainFrame::OnCreate(LPCREATESTRUCT createStructure)
{
    if (CFrameWnd::OnCreate(createStructure) == -1)
        return -1;

    if (!statusBar_.Create(this)
        || !statusBar_.SetIndicators(kStatusBarIndicators,
                                     _countof(kStatusBarIndicators))) {
        return -1;
    }

    updateEffectStatus();
    return 0;
}

void MainFrame::OnEffectSettings()
{
    EffectSettingsDialog dialog(effectSettings_, this);
    if (dialog.DoModal() == IDOK) {
        effectSettings_ = dialog.selectedSettings();
        updateEffectStatus();
    }
}

void MainFrame::updateEffectStatus()
{
    CString statusText;
    statusText.Format(_T("Selected effect: %s"),
                      effectTypeDisplayName(effectSettings_.selectedEffect));
    statusBar_.SetPaneText(0, statusText);
}

BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_COMMAND(ID_EFFECT_SETTINGS, &MainFrame::OnEffectSettings)
END_MESSAGE_MAP()
