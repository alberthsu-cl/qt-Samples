#pragma once

#include "EditorSession.h"
#include "MediaLibrary.h"
#include "ProjectSerializer.h"
#include "WorkspaceLayout.h"
#include "WorkspaceSettings.h"

#if MINI_EDITOR_USE_QT
#include "QtMediaLibraryHost.h"
#include "QtPropertiesHost.h"
#include "QtTimelineCanvasHost.h"
#include "QtTimelineToolbarHost.h"
#include "QtTransportHost.h"
#else
#include "MfcMediaLibraryPane.h"
#include "MfcTransportBar.h"
#include "MfcPropertiesPane.h"
#endif
#include "MfcPreviewCanvas.h"
#if !MINI_EDITOR_USE_QT
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
    afx_msg void OnEditSplitClip();
    afx_msg void OnUpdateEditUndo(CCmdUI *commandUi);
    afx_msg void OnUpdateEditRedo(CCmdUI *commandUi);
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
    void handlePlaybackCommand(PlaybackCommand command);
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
    PreviewState currentPreviewState() const;
    void synchronizePlaybackDurationForFocus(bool resetToBeginning);
    void importMediaFile();
    void removeMediaAsset(int assetIndex, int assetId);
    void focusTimelineClip(int clipId, bool resetToBeginning);
    void focusTimelineFrame(int frame);
    bool canSplitSelectedTimelineClip() const;
    void splitSelectedTimelineClip();

    CStatusBar statusBar_;
#if MINI_EDITOR_USE_QT
    QtMediaLibraryHost mediaLibraryHost_;
    QtPropertiesHost propertiesHost_;
    QtTransportHost transportHost_;
    QtTimelineCanvasHost timelineCanvasHost_;
    QtTimelineToolbarHost timelineToolbarHost_;
#else
    MfcMediaLibraryPane mediaLibraryPane_;
    MfcPropertiesPane propertiesPane_;
    MfcTransportBar transportBar_;
#endif
    MfcPreviewCanvas previewCanvas_;
#if MINI_EDITOR_USE_QT
    // Timeline is migrated to Qt in the Qt-enabled build.
#else
    MfcTimelinePane timelinePane_;
#endif
    MfcWorkspaceSplitter leftSplitter_;
    MfcWorkspaceSplitter rightSplitter_;
    MfcWorkspaceSplitter timelineSplitter_;
    MediaLibrary mediaLibrary_;
    int builtInMediaAssetCount_ = 0;
    EditorSession editorSession_;
    EditorSession::ObserverId editorSessionObserverId_ = 0;
    WorkspaceLayout workspaceLayout_;
    std::filesystem::path projectFilePath_;
    bool isWorkspaceReady_ = false;
};
