#pragma once

#include "EffectSettings.h"

#include <afxwin.h>
#include <afxext.h>
#include <afxcmn.h>

class MainFrame final : public CFrameWnd
{
    DECLARE_DYNCREATE(MainFrame)

protected:
    int OnCreate(LPCREATESTRUCT createStructure);
    afx_msg void OnEffectSettings();

    DECLARE_MESSAGE_MAP()

private:
    void updateEffectStatus();

    CStatusBar statusBar_;
    EffectSettings effectSettings_;
};
