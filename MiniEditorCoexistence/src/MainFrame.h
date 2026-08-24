#pragma once

#include "EditorSession.h"
#include "WorkspaceLayout.h"
#include "WorkspaceSettings.h"

#if MINI_EDITOR_USE_QT
#include "QtMediaLibraryHost.h"
#include "QtPropertiesHost.h"
#include "QtTimelineToolbarHost.h"
#include "QtTransportHost.h"
#else
#include "MfcMediaLibraryPane.h"
#include "MfcTransportBar.h"
#include "MfcPropertiesPane.h"
#endif
#include "MfcPreviewCanvas.h"
#include "MfcTimelineCanvas.h"
#include "MfcTimelinePane.h"
#include "MfcWorkspaceSplitter.h"

#include <afxcmn.h>
#include <afxext.h>
#include <afxwin.h>

class MainFrame final : public CFrameWnd
{
    DECLARE_DYNCREATE(MainFrame)

public:
    MainFrame();
    ~MainFrame() override;

protected:
    afx_msg int OnCreate(LPCREATESTRUCT createStructure);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO *minMaxInfo);
    afx_msg void OnSelectMediaAsset(UINT commandId);
    afx_msg void OnPlaybackCommand(UINT commandId);
    afx_msg void OnEditUndo();
    afx_msg void OnEditRedo();
    afx_msg void OnUpdateEditUndo(CCmdUI *commandUi);
    afx_msg void OnUpdateEditRedo(CCmdUI *commandUi);
    afx_msg void OnTimer(UINT_PTR timerId);
    afx_msg void OnFileExit();

    DECLARE_MESSAGE_MAP()

private:
    void layoutChildren(int clientWidth, int clientHeight);
    int contentBottomForClient(int clientHeight);
    void handlePlaybackCommand(PlaybackCommand command);
    void refreshEditorViews(EditorChange changes);
    void moveLeftSplitter(int parentX);
    void moveRightSplitter(int parentX);
    void moveTimelineSplitter(int parentY);
    void updateStatusText();
    void restoreWorkspaceSettings();
    void saveWorkspaceSettings() const;

    CStatusBar statusBar_;
#if MINI_EDITOR_USE_QT
    QtMediaLibraryHost mediaLibraryHost_;
    QtPropertiesHost propertiesHost_;
    QtTransportHost transportHost_;
    QtTimelineToolbarHost timelineToolbarHost_;
#else
    MfcMediaLibraryPane mediaLibraryPane_;
    MfcPropertiesPane propertiesPane_;
    MfcTransportBar transportBar_;
#endif
    MfcPreviewCanvas previewCanvas_;
#if MINI_EDITOR_USE_QT
    MfcTimelineCanvas timelineCanvas_;
#else
    MfcTimelinePane timelinePane_;
#endif
    MfcWorkspaceSplitter leftSplitter_;
    MfcWorkspaceSplitter rightSplitter_;
    MfcWorkspaceSplitter timelineSplitter_;
    EditorSession editorSession_;
    EditorSession::ObserverId editorSessionObserverId_ = 0;
    WorkspaceLayout workspaceLayout_;
    bool isWorkspaceReady_ = false;
};
