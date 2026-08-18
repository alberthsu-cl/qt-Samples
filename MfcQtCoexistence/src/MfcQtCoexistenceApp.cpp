#include "MainFrame.h"
#if MFCQT_USE_QT
#include "QtRuntime.h"
#endif
#include "resource.h"

#include <afxwin.h>

#include <memory>

class MfcQtCoexistenceApp final : public CWinApp
{
public:
    BOOL InitInstance() override;
    int ExitInstance() override;

private:
#if MFCQT_USE_QT
    std::unique_ptr<QtRuntime> qtRuntime_;
#endif
};

MfcQtCoexistenceApp theApp;

BOOL MfcQtCoexistenceApp::InitInstance()
{
    CWinApp::InitInstance();
    SetRegistryKey(_T("QtLearningSamples"));

    // Phase 2: create exactly one QApplication in this MFC process, before
    // constructing any Qt Widgets. MFC still owns the main application loop.
#if MFCQT_USE_QT
    qtRuntime_ = std::make_unique<QtRuntime>();
#endif

    auto *mainFrame = new MainFrame;
    m_pMainWnd = mainFrame;

    // Phase 1 has no Qt initialization. MFC owns the process, the main window,
    // the message loop, and this menu command.
    if (!mainFrame->Create(nullptr,
                           _T("MFC / Qt Coexistence - Phase 1"),
                           WS_OVERLAPPEDWINDOW,
                           // A predictable learning-sample size avoids the
                           // tiny default frame shown by CFrameWnd::rectDefault
                           // on some systems.
                           CRect(120, 120, 1120, 780),
                           nullptr,
                           MAKEINTRESOURCE(IDR_MAINFRAME))) {
        delete mainFrame;
        return FALSE;
    }

    mainFrame->ShowWindow(SW_SHOW);
    mainFrame->UpdateWindow();
    return TRUE;
}

int MfcQtCoexistenceApp::ExitInstance()
{
    // Destroy Qt after the MFC main window has completed its shutdown.
#if MFCQT_USE_QT
    qtRuntime_.reset();
#endif
    return CWinApp::ExitInstance();
}
