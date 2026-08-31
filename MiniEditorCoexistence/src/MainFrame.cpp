#include "MainFrame.h"

#include "DemoProject.h"
#include "ClipPropertiesStateResolver.h"
#include "PreviewStateResolver.h"
#include "TimelinePresentationStateResolver.h"
#include "resource.h"

#include <algorithm>
#include <array>
#include <cwchar>
#include <filesystem>

IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)

namespace {

constexpr UINT kStatusBarIndicators[] = { ID_SEPARATOR };
constexpr UINT_PTR kPlaybackTimerId = 1;

const wchar_t *mediaKindName(MediaKind kind)
{
    switch (kind) {
    case MediaKind::Video: return L"Video";
    case MediaKind::Audio: return L"Audio";
    case MediaKind::Image: return L"Image";
    }
    return L"Media";
}

CRect toClientRect(const WorkspaceRect &rect)
{
    return CRect(rect.left, rect.top, rect.left + rect.width, rect.top + rect.height);
}

EditorIntent commandForPlayback(PlaybackCommand command)
{
    switch (command) {
    case PlaybackCommand::TogglePlayPause:
        return EditorIntent::TogglePlayback;
    case PlaybackCommand::Stop:
        return EditorIntent::StopPlayback;
    case PlaybackCommand::StepBackward:
        return EditorIntent::StepBackward;
    case PlaybackCommand::StepForward:
        return EditorIntent::StepForward;
    }

    return EditorIntent::TogglePlayback;
}

} // namespace

MainFrame::MainFrame()
    : editorSession_(demoAssets().size()),
      timelineController_(editorSession_, mediaLibrary_),
#if MINI_EDITOR_USE_QT
      playbackBackend_(editorSession_, mediaLibrary_),
#else
      playbackBackend_(editorSession_),
#endif
      commandController_(editorSession_, timelineController_, playbackBackend_),
      documentService_(editorSession_, mediaLibrary_)
{
    // The sample starts with familiar built-in assets, but the Qt media
    // library now reads its display data from MediaLibrary rather than from
    // a hard-coded view model. Imported items will join this same catalog.
    for (const MediaAsset &asset : demoAssets()) {
        const MediaKind kind = wcscmp(asset.kind, L"Audio") == 0 ? MediaKind::Audio
            : wcscmp(asset.kind, L"Image") == 0 ? MediaKind::Image : MediaKind::Video;
        const std::uint32_t color = (static_cast<std::uint32_t>(GetRValue(asset.thumbnailColor)) << 16)
            | (static_cast<std::uint32_t>(GetGValue(asset.thumbnailColor)) << 8)
            | static_cast<std::uint32_t>(GetBValue(asset.thumbnailColor));
        mediaLibrary_.addKnownAsset(asset.name, kind, asset.timelineDurationFrames, color);
    }
    documentService_.setDefaultMediaLibrary(mediaLibrary_);
}

MainFrame::~MainFrame()
{
    if (editorSessionObserverId_ != 0)
        editorSession_.removeObserver(editorSessionObserverId_);

    if (::IsWindow(GetSafeHwnd()))
        KillTimer(kPlaybackTimerId);

    if (isWorkspaceReady_)
        saveWorkspaceSettings();
}

int MainFrame::OnCreate(LPCREATESTRUCT createStructure)
{
    if (CFrameWnd::OnCreate(createStructure) == -1)
        return -1;

    if (!statusBar_.Create(this)
        || !statusBar_.SetIndicators(kStatusBarIndicators,
                                     _countof(kStatusBarIndicators))) {
        return -1;
    }
    statusBar_.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, 400);
    restoreWorkspaceSettings();

    if (
#if MINI_EDITOR_USE_QT
        !previewHost_.create(GetSafeHwnd())
#else
        !previewCanvas_.Create(this, IDC_PREVIEW_CANVAS)
        || !timelinePane_.Create(this, IDC_TIMELINE)
#endif
        || !leftSplitter_.Create(MfcWorkspaceSplitter::Orientation::Vertical,
                                 this, IDC_LEFT_SPLITTER)
        || !rightSplitter_.Create(MfcWorkspaceSplitter::Orientation::Vertical,
                                  this, IDC_RIGHT_SPLITTER)
        || !timelineSplitter_.Create(MfcWorkspaceSplitter::Orientation::Horizontal,
                                     this, IDC_TIMELINE_SPLITTER)) {
        return -1;
    }

    leftSplitter_.setDragHandler([this](int parentX) { moveLeftSplitter(parentX); });
    rightSplitter_.setDragHandler([this](int parentX) { moveRightSplitter(parentX); });
    timelineSplitter_.setDragHandler([this](int parentY) { moveTimelineSplitter(parentY); });

#if MINI_EDITOR_USE_QT
    QObject::connect(&thumbnailCache_, &QtThumbnailCache::thumbnailChanged,
                     &thumbnailCache_, [this](int) {
                         refreshEditorViews(EditorChange::TimelineClip);
                     });
    thumbnailCache_.refresh(mediaLibrary_);
    playbackBackend_.setVideoOutput(previewHost_.videoSink());
    playbackBackend_.setVideoVisibilityHandler(
        [this](bool visible) { previewHost_.setDecodedVideoVisible(visible); });
    playbackBackend_.setSourceMetadataChangedHandler(
        [this] { refreshEditorViews(EditorChange::Selection); });

    if (!timelineCanvasHost_.create(GetSafeHwnd()))
        return -1;
    timelineCanvasHost_.setSeekHandler(
        [this](int frame) {
            timelineController_.focusFrame(frame);
            // Moving the timeline head changes editor state first, then seeks
            // the real decoder to the resolved source frame. Without this,
            // QMediaPlayer keeps playing from the frame it decoded when the
            // clip was selected (usually the clip's first frame).
            seekPreviewToCurrentFrame();
        });
    timelineCanvasHost_.setTimelineClipEditedHandler(
        [this](int clipId, const TimelineClipState &state,
               TimelineClipEditKind editKind) {
            if (editorSession_.moveTimelineClip(clipId, state, editKind))
                timelineController_.synchronizePlaybackDuration(false);
    });
    timelineCanvasHost_.setMediaAssetDroppedHandler(
        [this](int mediaAssetId, int frame) {
            timelineController_.insertMediaAsset(mediaAssetId, frame);
        });
    timelineCanvasHost_.setAssetPresentationResolver(
        [this](int mediaAssetId) -> std::optional<TimelineAssetPresentation> {
            const LibraryMediaAsset *asset = mediaLibrary_.findAsset(mediaAssetId);
            if (asset == nullptr)
                return std::nullopt;

            TimelineAssetPresentation presentation;
            presentation.displayName = QString::fromStdWString(asset->displayName);
            presentation.color = QColor::fromRgb(asset->thumbnailColorRgb);
            presentation.thumbnail = thumbnailCache_.imageFor(asset->id);
            presentation.trackType = asset->kind == MediaKind::Audio
                ? TimelineTrackType::Audio : TimelineTrackType::Video;
            presentation.mediaKind = asset->kind;
            presentation.durationFrames = asset->timelineDurationFrames;
            presentation.isRealAsset = !asset->filePath.empty();
            return presentation;
        });
    timelineCanvasHost_.setClipThumbnailResolver(
        [this](const TimelineClip &clip, int sourceFrame) {
            return thumbnailCache_.timelineImageFor(clip, sourceFrame);
        });
    timelineCanvasHost_.setTimelineClipDeletedHandler(
        [this](int clipId) {
            if (timelineController_.deleteClip(clipId))
                synchronizePlaybackTimer();
        });
    timelineCanvasHost_.setTimelineClipSelectedHandler(
        [this](int clipId) { timelineController_.focusClip(clipId, false); });
    timelineCanvasHost_.setTimelineFocusRequestedHandler(
        [this] { timelineController_.focusEmptyTimeline(); });
    timelineCanvasHost_.setAudioTrackVisibilityHandler([this](bool isVisible) {
        TimelineViewState state = editorSession_.timelineViewState();
        state.isAudioTrackVisible = isVisible;
        editorSession_.updateTimelineViewState(state);
    });
    if (!mediaLibraryHost_.create(GetSafeHwnd(), mediaLibrary_, &thumbnailCache_))
        return -1;
    if (!propertiesHost_.create(GetSafeHwnd()))
        return -1;
    if (!transportHost_.create(GetSafeHwnd()))
        return -1;
    if (!timelineToolbarHost_.create(GetSafeHwnd()))
        return -1;

    // Qt panels emit framework-neutral values. MainFrame remains the native
    // host and forwards session state to the migrated Qt panel boundaries.
    mediaLibraryHost_.setAssetSelectedHandler([this](int assetIndex) {
        timelineController_.selectSourceAsset(assetIndex);
    });
    mediaLibraryHost_.setImportHandler([this] { importMediaFile(); });
    mediaLibraryHost_.setRemoveHandler(
        [this](int assetIndex, int assetId) { removeMediaAsset(assetIndex, assetId); });
    propertiesHost_.setClipSettingsEditedHandler([this](const ClipSettings &settings) {
        editorSession_.updateSelectedClipSettings(settings);
    });
    transportHost_.setPlaybackCommandHandler([this](PlaybackCommand command) {
        executeEditorCommand(commandForPlayback(command));
    });
    transportHost_.setPlaybackPositionHandler(
        [this](int frame) {
            timelineController_.seekFocusedPreview(frame);
            seekPreviewToCurrentFrame();
        });
    timelineToolbarHost_.setViewStateEditedHandler([this](const TimelineViewState &state) {
        editorSession_.updateTimelineViewState(state);
    });
    timelineToolbarHost_.setFitTimelineHandler([this] { editorSession_.fitTimeline(); });
    timelineToolbarHost_.setSplitClipHandler(
            [this] { executeEditorCommand(EditorIntent::SplitClip); });
#else
    if (!mediaLibraryPane_.Create(this, IDC_MEDIA_LIBRARY)
        || !propertiesPane_.Create(this, IDC_PROPERTIES)
        || !transportBar_.Create(this, IDC_TRANSPORT)) {
        return -1;
    }
#endif

    // From here onward each framework-neutral session change refreshes every
    // active MFC/Qt view. MainFrame remains the composition and layout owner.
    editorSessionObserverId_ = editorSession_.addObserver(
        [this](EditorChange changes) { refreshEditorViews(changes); });
    refreshEditorViews(EditorChange::All);
    isWorkspaceReady_ = true;
    return 0;
}

BOOL MainFrame::OnEraseBkgnd(CDC *deviceContext)
{
    CRect clientRect;
    GetClientRect(&clientRect);
    // MFC owns the gaps between the hosted Qt HWNDs. Paint those layout gaps
    // explicitly so Windows never falls back to the bright system background.
    deviceContext->FillSolidRect(clientRect, RGB(29, 31, 36));
    return TRUE;
}

void MainFrame::OnSize(UINT type, int width, int height)
{
    CFrameWnd::OnSize(type, width, height);

    if (::IsWindow(statusBar_.GetSafeHwnd()))
        statusBar_.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, std::max(1, width));

    RecalcLayout();
    layoutChildren(width, height);
}

void MainFrame::OnGetMinMaxInfo(MINMAXINFO *minMaxInfo)
{
    CFrameWnd::OnGetMinMaxInfo(minMaxInfo);
    minMaxInfo->ptMinTrackSize.x = 1040;
    minMaxInfo->ptMinTrackSize.y = 680;
}

void MainFrame::OnSelectMediaAsset(UINT commandId)
{
    timelineController_.selectSourceAsset(
        static_cast<int>(commandId - ID_MEDIA_ASSET_FIRST));
}

void MainFrame::OnPlaybackCommand(UINT commandId)
{
    switch (commandId) {
    case ID_PLAYBACK_TOGGLE:
        executeEditorCommand(EditorIntent::TogglePlayback);
        break;
    case ID_PLAYBACK_STOP:
        executeEditorCommand(EditorIntent::StopPlayback);
        break;
    case ID_PLAYBACK_STEP_BACKWARD:
        executeEditorCommand(EditorIntent::StepBackward);
        break;
    case ID_PLAYBACK_STEP_FORWARD:
        executeEditorCommand(EditorIntent::StepForward);
        break;
    }
}

void MainFrame::OnEditUndo()
{
    executeEditorCommand(EditorIntent::Undo);
}

void MainFrame::OnEditRedo()
{
    executeEditorCommand(EditorIntent::Redo);
}

void MainFrame::OnEditCopyClip()
{
    executeEditorCommand(EditorIntent::CopyClip);
}

void MainFrame::OnEditCutClip()
{
    executeEditorCommand(EditorIntent::CutClip);
}

void MainFrame::OnEditPasteClip()
{
    executeEditorCommand(EditorIntent::PasteClip);
}

void MainFrame::OnEditDuplicateClip()
{
    executeEditorCommand(EditorIntent::DuplicateClip);
}

void MainFrame::OnEditSplitClip()
{
    executeEditorCommand(EditorIntent::SplitClip);
}

void MainFrame::OnUpdateEditUndo(CCmdUI *commandUi)
{
    commandUi->Enable(commandController_.canExecute(EditorIntent::Undo));
}

void MainFrame::OnUpdateEditRedo(CCmdUI *commandUi)
{
    commandUi->Enable(commandController_.canExecute(EditorIntent::Redo));
}

void MainFrame::OnUpdateEditCopyClip(CCmdUI *commandUi)
{
    commandUi->Enable(commandController_.canExecute(EditorIntent::CopyClip));
}

void MainFrame::OnUpdateEditCutClip(CCmdUI *commandUi)
{
    commandUi->Enable(commandController_.canExecute(EditorIntent::CutClip));
}

void MainFrame::OnUpdateEditPasteClip(CCmdUI *commandUi)
{
    commandUi->Enable(commandController_.canExecute(EditorIntent::PasteClip));
}

void MainFrame::OnUpdateEditDuplicateClip(CCmdUI *commandUi)
{
    commandUi->Enable(commandController_.canExecute(EditorIntent::DuplicateClip));
}

void MainFrame::OnUpdateEditSplitClip(CCmdUI *commandUi)
{
    commandUi->Enable(commandController_.canExecute(EditorIntent::SplitClip));
}

void MainFrame::OnTimer(UINT_PTR timerId)
{
    if (timerId == kPlaybackTimerId) {
        // The same timer also pumps Qt events during a stopped, silent
        // one-frame decode. Only real timeline playback is allowed to move
        // selection with the head; otherwise clicking a clip would be undone
        // by the first decoder-pump tick.
        const bool wasPlayingTimeline = editorSession_.isTimelineFocused()
            && editorSession_.timelinePlaybackState().isPlaying;
        // This is a deliberately simple MFC timer. In a production editor the
        // media engine would report its clock/playhead instead.
        applyPlaybackClockAction(playbackBackend_.advanceOneFrame());
        if (wasPlayingTimeline && editorSession_.isTimelineFocused())
            timelineController_.followPlaybackFrame();
        return;
    }

    CFrameWnd::OnTimer(timerId);
}

void MainFrame::OnFileExit()
{
    SendMessage(WM_CLOSE);
}

void MainFrame::OnClose()
{
    if (confirmSaveBeforeDestructiveAction())
        CFrameWnd::OnClose();
}

void MainFrame::OnFileNew()
{
    if (!confirmSaveBeforeDestructiveAction())
        return;

    const ProjectDocumentResult result = documentService_.createNewProject();
    if (!result.succeeded()) {
        AfxMessageBox(CString(result.message.c_str()), MB_ICONERROR | MB_OK);
        return;
    }
#if MINI_EDITOR_USE_QT
    mediaLibraryHost_.refreshAssets();
#endif
    projectFilePath_.clear();
    updateWindowTitle();
}

void MainFrame::OnFileOpen()
{
    if (!confirmSaveBeforeDestructiveAction())
        return;

    CFileDialog dialog(TRUE, L"mini-editor.json", nullptr,
                       OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                       L"Mini Editor Project (*.mini-editor.json)|*.mini-editor.json||", this);
    if (dialog.DoModal() != IDOK)
        return;

    openProject(std::filesystem::path(static_cast<LPCTSTR>(dialog.GetPathName())));
}

void MainFrame::OnFileSave()
{
    saveProject(false);
}

void MainFrame::OnFileSaveAs()
{
    saveProject(true);
}

void MainFrame::OnUpdateFileSave(CCmdUI *commandUi)
{
    commandUi->Enable(editorSession_.isProjectDirty());
}

BOOL MainFrame::PreTranslateMessage(MSG *message)
{
    // This frame is created with CFrameWnd::Create rather than LoadFrame, so
    // it does not automatically load the accelerator table from the resource.
    // Translate it explicitly before normal MFC message processing.
    static const HACCEL acceleratorTable = ::LoadAccelerators(
        AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME));
    if (acceleratorTable != nullptr
        && ::TranslateAccelerator(GetSafeHwnd(), acceleratorTable, message)) {
        return TRUE;
    }

    return CFrameWnd::PreTranslateMessage(message);
}

void MainFrame::layoutChildren(int clientWidth, int clientHeight)
{
#if !MINI_EDITOR_USE_QT
    if (!::IsWindow(mediaLibraryPane_.GetSafeHwnd())
        || !::IsWindow(propertiesPane_.GetSafeHwnd()))
        return;
#endif

    const WorkspaceGeometry geometry = workspaceLayout_.calculate(
        clientWidth, contentBottomForClient(clientHeight));

#if MINI_EDITOR_USE_QT
    mediaLibraryHost_.resize(toClientRect(geometry.mediaLibrary));
#else
    mediaLibraryPane_.MoveWindow(geometry.mediaLibrary.left, geometry.mediaLibrary.top,
                                 geometry.mediaLibrary.width, geometry.mediaLibrary.height);
#endif
#if MINI_EDITOR_USE_QT
    previewHost_.resize(toClientRect(geometry.previewCanvas));
    transportHost_.resize(toClientRect(geometry.transport));
#else
    previewCanvas_.MoveWindow(geometry.previewCanvas.left, geometry.previewCanvas.top,
                              geometry.previewCanvas.width, geometry.previewCanvas.height);
    transportBar_.MoveWindow(geometry.transport.left, geometry.transport.top,
                             geometry.transport.width, geometry.transport.height);
#endif
#if MINI_EDITOR_USE_QT
    propertiesHost_.resize(toClientRect(geometry.properties));
#else
    propertiesPane_.MoveWindow(geometry.properties.left, geometry.properties.top,
                               geometry.properties.width, geometry.properties.height);
#endif
#if MINI_EDITOR_USE_QT
    timelineToolbarHost_.resize(toClientRect(geometry.timelineToolbar));
    timelineCanvasHost_.resize(toClientRect(geometry.timelineCanvas));
#else
    timelinePane_.MoveWindow(geometry.timeline.left, geometry.timeline.top,
                             geometry.timeline.width, geometry.timeline.height);
#endif

    leftSplitter_.MoveWindow(geometry.leftSplitter.left, geometry.leftSplitter.top,
                             geometry.leftSplitter.width, geometry.leftSplitter.height);
    rightSplitter_.MoveWindow(geometry.rightSplitter.left, geometry.rightSplitter.top,
                              geometry.rightSplitter.width, geometry.rightSplitter.height);
    timelineSplitter_.MoveWindow(geometry.timelineSplitter.left, geometry.timelineSplitter.top,
                                 geometry.timelineSplitter.width,
                                 geometry.timelineSplitter.height);
}

int MainFrame::contentBottomForClient(int clientHeight)
{
    if (!::IsWindow(statusBar_.GetSafeHwnd()))
        return clientHeight;

    CRect statusRect;
    statusBar_.GetWindowRect(&statusRect);
    ScreenToClient(&statusRect);
    return std::clamp(static_cast<int>(statusRect.top), 0, clientHeight);
}

void MainFrame::executeEditorCommand(EditorIntent command)
{
    const EditorCommandResult result = commandController_.execute(command);
    if (result.playbackTimerNeedsSync)
        synchronizePlaybackTimer();
}

void MainFrame::synchronizePlaybackTimer()
{
    applyPlaybackClockAction(playbackBackend_.synchronize());
}

void MainFrame::seekPreviewToCurrentFrame()
{
    const PreviewSeekRequest request = PreviewSeekRequestResolver::resolve(
        nextPreviewSeekRequestId_++, editorSession_, mediaLibrary_);
    applyPlaybackClockAction(playbackBackend_.seek(request));
}

void MainFrame::applyPlaybackClockAction(PlaybackClockAction action)
{
    if (action == PlaybackClockAction::EnsureRunning)
        SetTimer(kPlaybackTimerId, playbackBackend_.tickIntervalMilliseconds(), nullptr);
    else
        KillTimer(kPlaybackTimerId);
}

void MainFrame::refreshEditorViews(EditorChange changes)
{
    const int selectedAssetIndex = editorSession_.selectedAssetIndex();
    const ClipSettings &settings = editorSession_.selectedClipSettings();
    const PlaybackState &playbackState = editorSession_.playbackState();
    const ClipPropertiesViewState propertiesViewState =
        ClipPropertiesStateResolver::resolve(editorSession_, mediaLibrary_);
    const bool selectionChanged = includesChange(changes, EditorChange::Selection);
    const bool clipSettingsChanged = includesChange(changes, EditorChange::ClipSettings);
    const bool playbackChanged = includesChange(changes, EditorChange::Playback);
    const bool timelineViewChanged = includesChange(changes, EditorChange::TimelineView);
    const bool timelineClipChanged = includesChange(changes, EditorChange::TimelineClip);

#if MINI_EDITOR_USE_QT
    const TimelinePresentationState timelinePresentationState =
        TimelinePresentationStateResolver::resolve(
            editorSession_, commandController_.canExecute(EditorIntent::SplitClip));
    // Effect-aware clip thumbnails are prepared during state refresh. The
    // canvas resolver below only performs a cache lookup while painting.
    thumbnailCache_.prepareTimelineThumbnails(
        timelinePresentationState.clips,
        timelinePresentationState.view.zoomPercent);
    if (selectionChanged) {
        if (editorSession_.isTimelineFocused())
            mediaLibraryHost_.clearSelection();
        else if (selectedAssetIndex < 0)
            mediaLibraryHost_.clearSelection();
        else
            mediaLibraryHost_.setSelectedAssetIndex(selectedAssetIndex);
    }
    // A trim reports TimelineClip rather than ClipSettings, yet it changes the
    // room available for fades, so the inspector refreshes for that too.
    if (selectionChanged || clipSettingsChanged || timelineClipChanged) {
        // QtPropertiesPanel uses QSignalBlocker while it receives this state,
        // so an editor-to-view refresh never loops back as a user request.
        propertiesHost_.setViewState(propertiesViewState);
    }
    if (selectionChanged || clipSettingsChanged || playbackChanged
        || timelineViewChanged || timelineClipChanged) {
        timelineCanvasHost_.setPresentationState(timelinePresentationState);
        timelineToolbarHost_.setPresentationState(timelinePresentationState);
    }
#else
    if (selectionChanged) {
        mediaLibraryPane_.setSelectedAssetIndex(selectedAssetIndex);
    }
    if (selectionChanged || clipSettingsChanged || timelineClipChanged)
        propertiesPane_.setViewState(propertiesViewState);
#endif

    if (selectionChanged) {
#if !MINI_EDITOR_USE_QT
        previewCanvas_.setSelectedAssetIndex(std::min(
            selectedAssetIndex, documentService_.protectedMediaAssetCount() - 1));
        timelinePane_.setSelectedAssetIndex(selectedAssetIndex);
#endif
    }
    if (selectionChanged || clipSettingsChanged) {
#if !MINI_EDITOR_USE_QT
        previewCanvas_.setClipSettings(settings);
        timelinePane_.setClipSettings(settings);
#endif
    }
    if (playbackChanged) {
#if MINI_EDITOR_USE_QT
        previewHost_.setPlaybackState(playbackState);
#else
        previewCanvas_.setPlaybackState(playbackState);
#endif
    }
#if MINI_EDITOR_USE_QT
    if (playbackChanged) {
        transportHost_.setPlaybackState(playbackState);
    }
#else
    if (playbackChanged) {
        timelinePane_.setPlaybackState(playbackState);
        transportBar_.setPlaybackState(playbackState);
    }
#endif
    if (selectionChanged || clipSettingsChanged || playbackChanged || timelineClipChanged) {
        const PreviewState preview = PreviewStateResolver::resolve(editorSession_, mediaLibrary_);
#if MINI_EDITOR_USE_QT
        // A cached image is the still-image source and also the temporary
        // preview for a video while QMediaPlayer loads its first decoded frame.
        previewHost_.setFallbackImage(
            thumbnailCache_.imageFor(preview.mediaAssetId));
        previewHost_.setPreviewState(preview);
#else
        previewCanvas_.setPreviewState(preview);
#endif
    }
    if (selectionChanged || clipSettingsChanged || playbackChanged)
        updateStatusText();
    if (clipSettingsChanged || timelineClipChanged)
        updateWindowTitle();
    if (selectionChanged)
        synchronizePlaybackTimer();
}

void MainFrame::moveLeftSplitter(int parentX)
{
    workspaceLayout_.moveLeftSplitter(parentX);
    CRect clientRect;
    GetClientRect(&clientRect);
    layoutChildren(clientRect.Width(), clientRect.Height());
}

void MainFrame::moveRightSplitter(int parentX)
{
    CRect clientRect;
    GetClientRect(&clientRect);
    workspaceLayout_.moveRightSplitter(parentX, clientRect.Width());
    layoutChildren(clientRect.Width(), clientRect.Height());
}

void MainFrame::moveTimelineSplitter(int parentY)
{
    CRect clientRect;
    GetClientRect(&clientRect);
    workspaceLayout_.moveTimelineSplitter(parentY,
                                          contentBottomForClient(clientRect.Height()));
    layoutChildren(clientRect.Width(), clientRect.Height());
}

void MainFrame::updateStatusText()
{
    const int selectedAssetIndex = editorSession_.selectedAssetIndex();
    if (selectedAssetIndex < 0
        || selectedAssetIndex >= static_cast<int>(mediaLibrary_.assets().size())) {
        statusBar_.SetPaneText(0, _T("No item selected"));
        return;
    }

    const LibraryMediaAsset &asset = mediaLibrary_.assets()[selectedAssetIndex];
    CString statusText;
    const ClipSettings &settings = editorSession_.selectedClipSettings();
    statusText.Format(_T("Selected: %s (%s) | Opacity %d%% | Scale %d%% | %s | Frame %d"),
                      asset.displayName.c_str(), mediaKindName(asset.kind), settings.opacityPercent,
                      settings.scalePercent, clipPositionDisplayName(settings.position),
                      editorSession_.playbackState().currentFrame);
    statusBar_.SetPaneText(0, statusText);
}

void MainFrame::restoreWorkspaceSettings()
{
    const auto settings = WorkspaceSettingsStore::load();
    if (!settings)
        return;

    // Values are clamped during the first layout, when the actual frame size
    // is known. This also protects us from an old or manually edited JSON file.
    workspaceLayout_.setState({ settings->mediaLibraryWidth,
                                settings->propertiesWidth,
                                settings->timelineHeight });
    editorSession_.restoreWorkspaceState(settings->timelineViewState);
}

void MainFrame::saveWorkspaceSettings() const
{
    WorkspaceSettings settings;
    const WorkspaceLayoutState layoutState = workspaceLayout_.state();
    settings.mediaLibraryWidth = layoutState.mediaLibraryWidth;
    settings.propertiesWidth = layoutState.propertiesWidth;
    settings.timelineHeight = layoutState.timelineHeight;
    settings.timelineViewState = editorSession_.timelineViewState();
    WorkspaceSettingsStore::save(settings);
}

void MainFrame::importMediaFile()
{
    // MFC requires caller-owned storage for the null-separated file list that
    // OFN_ALLOWMULTISELECT returns. The normal CFileDialog buffer is only
    // large enough for one path.
    std::array<wchar_t, 64 * 1024> selectedFileBuffer{};
    CFileDialog dialog(TRUE, nullptr, nullptr,
        OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER,
        L"Media files (*.mp4;*.mov;*.mkv;*.avi;*.mp3;*.wav;*.m4a;*.aac;*.jpg;*.jpeg;*.png;*.bmp)|*.mp4;*.mov;*.mkv;*.avi;*.mp3;*.wav;*.m4a;*.aac;*.jpg;*.jpeg;*.png;*.bmp||",
        this);
    dialog.m_ofn.lpstrFile = selectedFileBuffer.data();
    dialog.m_ofn.nMaxFile = static_cast<DWORD>(selectedFileBuffer.size());
    if (dialog.DoModal() != IDOK)
        return;

    int importedFileCount = 0;
    CString failures;
    for (POSITION position = dialog.GetStartPosition(); position != nullptr;) {
        const std::filesystem::path path(
            static_cast<LPCTSTR>(dialog.GetNextPathName(position)));
        const ProjectDocumentResult result = documentService_.importMedia(path);
        if (result.succeeded()) {
            ++importedFileCount;
        } else {
            if (!failures.IsEmpty())
                failures += _T("\n");
            failures += path.filename().c_str();
            failures += _T(": ");
            failures += result.message.c_str();
        }
    }

#if MINI_EDITOR_USE_QT
    if (importedFileCount > 0)
        mediaLibraryHost_.refreshAssets();
#endif
    if (!failures.IsEmpty())
        AfxMessageBox(failures, MB_ICONWARNING | MB_OK);
}

void MainFrame::removeMediaAsset(int assetIndex, int assetId)
{
    const ProjectDocumentResult result = documentService_.removeMedia(assetIndex, assetId);
    if (result.succeeded()) {
#if MINI_EDITOR_USE_QT
        mediaLibraryHost_.refreshAssets();
#endif
        return;
    }

    const UINT icon = result.error == ProjectDocumentError::ProtectedMedia
        ? MB_ICONINFORMATION : MB_ICONWARNING;
    AfxMessageBox(CString(result.message.c_str()), icon | MB_OK);
}

bool MainFrame::saveProject(bool chooseFilePath)
{
    std::filesystem::path path = projectFilePath_;
    if (chooseFilePath || projectFilePath_.empty()) {
        CFileDialog dialog(FALSE, L"mini-editor.json", L"Untitled.mini-editor.json",
                           OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                           L"Mini Editor Project (*.mini-editor.json)|*.mini-editor.json||", this);
        if (dialog.DoModal() != IDOK)
            return false;

        path = std::filesystem::path(static_cast<LPCTSTR>(dialog.GetPathName()));
    }

    const ProjectDocumentResult result = documentService_.save(path);
    if (result.succeeded()) {
        projectFilePath_ = std::move(path);
        updateWindowTitle();
        return true;
    }

    AfxMessageBox(CString(result.message.c_str()), MB_ICONERROR | MB_OK);
    return false;
}

bool MainFrame::openProject(const std::filesystem::path &path)
{
    const ProjectDocumentResult result = documentService_.load(path);
    if (!result.succeeded()) {
        AfxMessageBox(CString(result.message.c_str()), MB_ICONERROR | MB_OK);
        return false;
    }
#if MINI_EDITOR_USE_QT
    mediaLibraryHost_.refreshAssets();
#endif
    // Opening a project enters timeline-preview mode. At frame 0 this selects
    // the first visible clip, or visibly focuses the empty timeline until a
    // later clip is reached. Playback therefore never starts ambiguously from
    // a stale library selection.
    timelineController_.focusFrame(0);
    projectFilePath_ = path;
    updateWindowTitle();
    return true;
}

void MainFrame::updateWindowTitle()
{
    CString title(_T("Mini Editor Coexistence"));
    if (projectFilePath_.empty()) {
        title += _T(" - Untitled Project");
    } else {
        title += _T(" - ");
        title += projectFilePath_.filename().c_str();
    }
    if (editorSession_.isProjectDirty())
        title += _T(" *");
    SetWindowText(title);
}

bool MainFrame::confirmSaveBeforeDestructiveAction()
{
    if (!editorSession_.isProjectDirty())
        return true;

    const int answer = AfxMessageBox(
        _T("The project has unsaved changes. Save them before continuing?"),
        MB_YESNOCANCEL | MB_ICONQUESTION);
    if (answer == IDCANCEL)
        return false;
    if (answer == IDYES)
        return saveProject(false);
    return true;
}

BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_COMMAND_RANGE(ID_MEDIA_ASSET_FIRST, ID_MEDIA_ASSET_LAST,
                     &MainFrame::OnSelectMediaAsset)
    ON_COMMAND_RANGE(ID_PLAYBACK_TOGGLE, ID_PLAYBACK_STEP_FORWARD,
                     &MainFrame::OnPlaybackCommand)
    ON_COMMAND(ID_EDIT_UNDO, &MainFrame::OnEditUndo)
    ON_COMMAND(ID_EDIT_REDO, &MainFrame::OnEditRedo)
    ON_COMMAND(ID_EDIT_COPY_CLIP, &MainFrame::OnEditCopyClip)
    ON_COMMAND(ID_EDIT_CUT_CLIP, &MainFrame::OnEditCutClip)
    ON_COMMAND(ID_EDIT_PASTE_CLIP, &MainFrame::OnEditPasteClip)
    ON_COMMAND(ID_EDIT_DUPLICATE_CLIP, &MainFrame::OnEditDuplicateClip)
    ON_COMMAND(ID_EDIT_SPLIT_CLIP, &MainFrame::OnEditSplitClip)
    ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, &MainFrame::OnUpdateEditUndo)
    ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, &MainFrame::OnUpdateEditRedo)
    ON_UPDATE_COMMAND_UI(ID_EDIT_COPY_CLIP, &MainFrame::OnUpdateEditCopyClip)
    ON_UPDATE_COMMAND_UI(ID_EDIT_CUT_CLIP, &MainFrame::OnUpdateEditCutClip)
    ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE_CLIP, &MainFrame::OnUpdateEditPasteClip)
    ON_UPDATE_COMMAND_UI(ID_EDIT_DUPLICATE_CLIP,
                         &MainFrame::OnUpdateEditDuplicateClip)
    ON_UPDATE_COMMAND_UI(ID_EDIT_SPLIT_CLIP, &MainFrame::OnUpdateEditSplitClip)
    ON_WM_TIMER()
    ON_COMMAND(ID_FILE_NEW, &MainFrame::OnFileNew)
    ON_COMMAND(ID_FILE_OPEN, &MainFrame::OnFileOpen)
    ON_COMMAND(ID_FILE_SAVE, &MainFrame::OnFileSave)
    ON_COMMAND(ID_FILE_SAVE_AS, &MainFrame::OnFileSaveAs)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, &MainFrame::OnUpdateFileSave)
    ON_COMMAND(ID_FILE_EXIT, &MainFrame::OnFileExit)
    ON_WM_CLOSE()
END_MESSAGE_MAP()
