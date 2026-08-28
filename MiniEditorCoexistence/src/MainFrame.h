#pragma once

#include "EditorSession.h"
#include "EditorCommandController.h"
#include "PlaybackBackend.h"
#include "MediaLibrary.h"
#include "ProjectDocumentService.h"
#include "TimelineEditingController.h"
#include "WorkspaceLayout.h"
#include "WorkspaceSettings.h"

#if MINI_EDITOR_USE_QT
#include "QtMediaLibraryHost.h"
#include "QtPreviewHost.h"
#include "QtPropertiesHost.h"
#include "QtTimelineCanvasHost.h"
#include "QtTimelineToolbarHost.h"
#include "QtTransportHost.h"
#else
#include "MfcMediaLibraryPane.h"
#include "MfcTransportBar.h"
#include "MfcPropertiesPane.h"
#endif
#if !MINI_EDITOR_USE_QT
#include "MfcPreviewCanvas.h"
#include "MfcTimelinePane.h"
#endif
#include "MfcWorkspaceSplitter.h"

#include <afxcmn.h>
#include <afxext.h>
#include <afxwin.h>

#include <filesystem>

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
    afx_msg void OnEditCopyClip();
    afx_msg void OnEditCutClip();
    afx_msg void OnEditPasteClip();
    afx_msg void OnEditDuplicateClip();
    afx_msg void OnEditSplitClip();
    afx_msg void OnUpdateEditUndo(CCmdUI *commandUi);
    afx_msg void OnUpdateEditRedo(CCmdUI *commandUi);
    afx_msg void OnUpdateEditCopyClip(CCmdUI *commandUi);
    afx_msg void OnUpdateEditCutClip(CCmdUI *commandUi);
    afx_msg void OnUpdateEditPasteClip(CCmdUI *commandUi);
    afx_msg void OnUpdateEditDuplicateClip(CCmdUI *commandUi);
    afx_msg void OnUpdateEditSplitClip(CCmdUI *commandUi);
    afx_msg void OnTimer(UINT_PTR timerId);
    afx_msg void OnFileExit();
    afx_msg void OnFileNew();
    afx_msg void OnFileOpen();
    afx_msg void OnFileSave();
    afx_msg void OnFileSaveAs();
    afx_msg void OnClose();
    afx_msg void OnUpdateFileSave(CCmdUI *commandUi);
    BOOL PreTranslateMessage(MSG *message) override;

    DECLARE_MESSAGE_MAP()

private:
    void layoutChildren(int clientWidth, int clientHeight);
    int contentBottomForClient(int clientHeight);
    void executeEditorCommand(EditorIntent command);
    void applyPlaybackClockAction(PlaybackClockAction action);
    void synchronizePlaybackTimer();
    void refreshEditorViews(EditorChange changes);
    void moveLeftSplitter(int parentX);
    void moveRightSplitter(int parentX);
    void moveTimelineSplitter(int parentY);
    void updateStatusText();
    void restoreWorkspaceSettings();
    void saveWorkspaceSettings() const;
    bool saveProject(bool chooseFilePath);
    bool openProject(const std::filesystem::path &path);
    bool confirmSaveBeforeDestructiveAction();
    void updateWindowTitle();
    void importMediaFile();
    void removeMediaAsset(int assetIndex, int assetId);

    CStatusBar statusBar_;
#if MINI_EDITOR_USE_QT
    QtMediaLibraryHost mediaLibraryHost_;
    QtPreviewHost previewHost_;
    QtPropertiesHost propertiesHost_;
    QtTransportHost transportHost_;
    QtTimelineCanvasHost timelineCanvasHost_;
    QtTimelineToolbarHost timelineToolbarHost_;
#else
    MfcMediaLibraryPane mediaLibraryPane_;
    MfcPropertiesPane propertiesPane_;
    MfcTransportBar transportBar_;
    MfcPreviewCanvas previewCanvas_;
#endif
#if MINI_EDITOR_USE_QT
    // Timeline is migrated to Qt in the Qt-enabled build.
#else
    MfcTimelinePane timelinePane_;
#endif
    MfcWorkspaceSplitter leftSplitter_;
    MfcWorkspaceSplitter rightSplitter_;
    MfcWorkspaceSplitter timelineSplitter_;
    MediaLibrary mediaLibrary_;
    EditorSession editorSession_;
    TimelineEditingController timelineController_;
    SimulatedPlaybackBackend playbackBackend_;
    EditorCommandController commandController_;
    ProjectDocumentService documentService_;
    EditorSession::ObserverId editorSessionObserverId_ = 0;
    WorkspaceLayout workspaceLayout_;
    std::filesystem::path projectFilePath_;
    bool isWorkspaceReady_ = false;
};
