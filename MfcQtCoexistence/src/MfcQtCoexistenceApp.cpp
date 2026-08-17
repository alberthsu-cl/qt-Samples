#include "MainFrame.h"
#include "resource.h"

#include <afxwin.h>

class MfcQtCoexistenceApp final : public CWinApp
{
public:
    BOOL InitInstance() override;
};

MfcQtCoexistenceApp theApp;

BOOL MfcQtCoexistenceApp::InitInstance()
{
    CWinApp::InitInstance();
    SetRegistryKey(_T("QtLearningSamples"));

    auto *mainFrame = new MainFrame;
    m_pMainWnd = mainFrame;

    // Phase 1 has no Qt initialization. MFC owns the process, the main window,
    // the message loop, and this menu command.
    if (!mainFrame->Create(nullptr,
                           _T("MFC / Qt Coexistence - Phase 1"),
                           WS_OVERLAPPEDWINDOW,
                           CFrameWnd::rectDefault,
                           nullptr,
                           MAKEINTRESOURCE(IDR_MAINFRAME))) {
        delete mainFrame;
        return FALSE;
    }

    mainFrame->ShowWindow(SW_SHOW);
    mainFrame->UpdateWindow();
    return TRUE;
}
