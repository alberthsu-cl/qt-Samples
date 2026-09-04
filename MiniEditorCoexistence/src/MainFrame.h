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
#include "QtAudioWaveformCache.h"
#include "QtMediaPlaybackBackend.h"
#include "QtThumbnailCache.h"
#if MINI_EDITOR_ENABLE_ENGINE_SMOKE_TEST
#include "EngineSmokeTestSession.h"
#endif
#if MINI_EDITOR_ENABLE_ENGINE_ROUTING
#include "TimelineEngineRouter.h"
#endif
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

#include <cstdint>
#include <filesystem>
#include <memory>

// ADR-007's MFC notification bridge (M4-05): "the MFC host posts one Windows
// message to MainFrame when the UI notification queue becomes non-empty."
// No such message existed before Milestone 4 -- this is the first one.
constexpr UINT WM_PLAYBACK_ENGINE_NOTIFICATION = WM_APP + 1;
// M4-06's routed timeline engine posts on a distinct message so it never
// cross-talks with the M4-04/M4-05 smoke test's own bridge/message, even in
// a build that happens to compile both in at once.
constexpr UINT WM_TIMELINE_ENGINE_NOTIFICATION = WM_APP + 2;

class MainFrame final : public CFrameWnd
{
    DECLARE_DYNCREATE(MainFrame)

public:
    MainFrame();
    ~MainFrame() override;

protected:
    afx_msg LRESULT OnPlaybackEngineNotification(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTimelineEngineNotification(WPARAM wParam, LPARAM lParam);
    afx_msg int OnCreate(LPCREATESTRUCT createStructure);
    afx_msg BOOL OnEraseBkgnd(CDC *deviceContext);
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
    void seekPreviewToCurrentFrame();
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
    // True only when compiled with MINI_EDITOR_ENABLE_ENGINE_ROUTING and the
    // timeline is currently focused -- the single condition every M4-06
    // routing decision below is gated on. Declared unconditionally so call
    // sites need no #if of their own; trivially false when not compiled in.
    bool isTimelineEngineRoutingActive() const;
    void updateTimelineEngineSnapshot(EditorChange changes);

    CStatusBar statusBar_;
#if MINI_EDITOR_USE_QT
    QtAudioWaveformCache waveformCache_;
    QtThumbnailCache thumbnailCache_;
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
#if MINI_EDITOR_USE_QT
    QtMediaPlaybackBackend playbackBackend_;
#else
    SimulatedPlaybackBackend playbackBackend_;
#endif
    EditorCommandController commandController_;
    ProjectDocumentService documentService_;
    EditorSession::ObserverId editorSessionObserverId_ = 0;
    std::uint64_t nextPreviewSeekRequestId_ = 1;
    WorkspaceLayout workspaceLayout_;
    std::filesystem::path projectFilePath_;
    bool isWorkspaceReady_ = false;
#if MINI_EDITOR_USE_QT && MINI_EDITOR_ENABLE_ENGINE_SMOKE_TEST
    // Milestone 4 manual validation only (M4-04/M4-05) -- unreachable except
    // through the debug hotkey in PreTranslateMessage. Never touches
    // playbackBackend_ or any production playback state.
    std::unique_ptr<EngineSmokeTestSession> engineSmokeTestSession_;
    void toggleEngineSmokeTest();
#endif
#if MINI_EDITOR_USE_QT && MINI_EDITOR_ENABLE_ENGINE_ROUTING
    // M4-06: routes timeline preview through the new engine. Constructed in
    // OnCreate, fed a fresh snapshot on every relevant EditorSession change.
    // Source preview and playbackBackend_ are never touched by this.
    std::unique_ptr<TimelineEngineRouter> timelineEngineRouter_;
#endif
};
