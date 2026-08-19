#include "MainFrame.h"

#include "resource.h"

#include <afxwin.h>

class MiniEditorCoexistenceApp final : public CWinApp
{
public:
    BOOL InitInstance() override;
};

MiniEditorCoexistenceApp theApp;

BOOL MiniEditorCoexistenceApp::InitInstance()
{
    CWinApp::InitInstance();
    SetRegistryKey(_T("QtLearningSamples"));

    auto *mainFrame = new MainFrame;
    m_pMainWnd = mainFrame;

    if (!mainFrame->Create(nullptr,
                           _T("Mini Editor Coexistence - Phase 0 (MFC)"),
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                           CRect(90, 80, 1550, 980),
                           nullptr,
                           MAKEINTRESOURCE(IDR_MAINFRAME))) {
        delete mainFrame;
        return FALSE;
    }

    mainFrame->ShowWindow(SW_SHOW);
    mainFrame->UpdateWindow();
    return TRUE;
}
