#include "MainFrame.h"
#if MINI_EDITOR_USE_QT_MEDIA_LIBRARY
#include "QtRuntime.h"
#endif

#include "resource.h"

#include <afxwin.h>

#include <memory>

class MiniEditorCoexistenceApp final : public CWinApp
{
public:
    BOOL InitInstance() override;
    int ExitInstance() override;

private:
#if MINI_EDITOR_USE_QT_MEDIA_LIBRARY
    std::unique_ptr<QtRuntime> qtRuntime_;
#endif
};

MiniEditorCoexistenceApp theApp;

BOOL MiniEditorCoexistenceApp::InitInstance()
{
    CWinApp::InitInstance();
    SetRegistryKey(_T("QtLearningSamples"));

#if MINI_EDITOR_USE_QT_MEDIA_LIBRARY
    // MFC still owns the normal application loop. QApplication initializes Qt
    // Widgets before the embedded Media Library is constructed.
    qtRuntime_ = std::make_unique<QtRuntime>();
#endif

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

int MiniEditorCoexistenceApp::ExitInstance()
{
#if MINI_EDITOR_USE_QT_MEDIA_LIBRARY
    qtRuntime_.reset();
#endif
    return CWinApp::ExitInstance();
}
