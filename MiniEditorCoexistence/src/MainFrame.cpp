#include "MainFrame.h"

#include "DemoProject.h"
#include "resource.h"

#include <algorithm>

IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)

namespace {

constexpr UINT kStatusBarIndicators[] = { ID_SEPARATOR };
constexpr UINT_PTR kPlaybackTimerId = 1;

CRect toClientRect(const WorkspaceRect &rect)
{
    return CRect(rect.left, rect.top, rect.left + rect.width, rect.top + rect.height);
}

} // namespace

MainFrame::MainFrame()
    : editorSession_(demoAssets().size())
{
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

    if (!previewCanvas_.Create(this, IDC_PREVIEW_CANVAS)
#if MINI_EDITOR_USE_QT
        || !timelineCanvas_.Create(this, IDC_TIMELINE)
#else
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
    timelineCanvas_.setSeekHandler([this](int frame) { editorSession_.seekTimeline(frame); });
    timelineCanvas_.setTimelineClipEditedHandler([this](const TimelineClipState &state) {
        editorSession_.updateSelectedTimelineClipState(state);
    });
    if (!mediaLibraryHost_.create(GetSafeHwnd()))
        return -1;
    if (!propertiesHost_.create(GetSafeHwnd()))
        return -1;
    if (!transportHost_.create(GetSafeHwnd()))
        return -1;
    if (!timelineToolbarHost_.create(GetSafeHwnd()))
        return -1;

    // Qt panels emit framework-neutral values. MFC continues to own selection
    // and clip settings, then redraws the remaining MFC panes from that state.
    mediaLibraryHost_.setAssetSelectedHandler([this](int assetIndex) {
        editorSession_.selectAsset(assetIndex);
    });
    propertiesHost_.setClipSettingsEditedHandler([this](const ClipSettings &settings) {
        editorSession_.updateSelectedClipSettings(settings);
    });
    transportHost_.setPlaybackCommandHandler([this](PlaybackCommand command) {
        handlePlaybackCommand(command);
    });
    timelineToolbarHost_.setViewStateEditedHandler([this](const TimelineViewState &state) {
        editorSession_.updateTimelineViewState(state);
    });
    timelineToolbarHost_.setFitTimelineHandler([this] { editorSession_.fitTimeline(); });
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
    minMaxInfo->ptMinTrackSize.x = 980;
    minMaxInfo->ptMinTrackSize.y = 680;
}

void MainFrame::OnSelectMediaAsset(UINT commandId)
{
    editorSession_.selectAsset(static_cast<int>(commandId - ID_MEDIA_ASSET_FIRST));
}

void MainFrame::OnPlaybackCommand(UINT commandId)
{
    switch (commandId) {
    case ID_PLAYBACK_TOGGLE:
        handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
        break;
    case ID_PLAYBACK_STOP:
        handlePlaybackCommand(PlaybackCommand::Stop);
        break;
    case ID_PLAYBACK_STEP_BACKWARD:
        handlePlaybackCommand(PlaybackCommand::StepBackward);
        break;
    case ID_PLAYBACK_STEP_FORWARD:
        handlePlaybackCommand(PlaybackCommand::StepForward);
        break;
    }
}

void MainFrame::OnEditUndo()
{
    editorSession_.undo();
}

void MainFrame::OnEditRedo()
{
    editorSession_.redo();
}

void MainFrame::OnUpdateEditUndo(CCmdUI *commandUi)
{
    commandUi->Enable(editorSession_.canUndo());
}

void MainFrame::OnUpdateEditRedo(CCmdUI *commandUi)
{
    commandUi->Enable(editorSession_.canRedo());
}

void MainFrame::OnTimer(UINT_PTR timerId)
{
    if (timerId == kPlaybackTimerId && editorSession_.playbackState().isPlaying) {
        // This is a deliberately simple MFC timer. In a production editor the
        // media engine would report its clock/playhead instead.
        editorSession_.advancePlaybackFrame();
        return;
    }

    CFrameWnd::OnTimer(timerId);
}

void MainFrame::OnFileExit()
{
    SendMessage(WM_CLOSE);
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
    previewCanvas_.MoveWindow(geometry.previewCanvas.left, geometry.previewCanvas.top,
                              geometry.previewCanvas.width, geometry.previewCanvas.height);
#if MINI_EDITOR_USE_QT
    transportHost_.resize(toClientRect(geometry.transport));
#else
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
    timelineCanvas_.MoveWindow(geometry.timelineCanvas.left, geometry.timelineCanvas.top,
                               geometry.timelineCanvas.width, geometry.timelineCanvas.height);
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

void MainFrame::handlePlaybackCommand(PlaybackCommand command)
{
    editorSession_.handlePlaybackCommand(command);
    if (editorSession_.playbackState().isPlaying)
        SetTimer(kPlaybackTimerId, 33, nullptr);
    else
        KillTimer(kPlaybackTimerId);
}

void MainFrame::refreshEditorViews(EditorChange changes)
{
    const int selectedAssetIndex = editorSession_.selectedAssetIndex();
    const auto &asset = demoAssets()[selectedAssetIndex];
    const ClipSettings &settings = editorSession_.selectedClipSettings();
    const PlaybackState &playbackState = editorSession_.playbackState();
    const TimelineViewState &timelineViewState = editorSession_.timelineViewState();
    const TimelineClipState &timelineClipState = editorSession_.selectedTimelineClipState();
    const bool selectionChanged = includesChange(changes, EditorChange::Selection);
    const bool clipSettingsChanged = includesChange(changes, EditorChange::ClipSettings);
    const bool playbackChanged = includesChange(changes, EditorChange::Playback);
    const bool timelineViewChanged = includesChange(changes, EditorChange::TimelineView);
    const bool timelineClipChanged = includesChange(changes, EditorChange::TimelineClip);

#if MINI_EDITOR_USE_QT
    if (selectionChanged)
        mediaLibraryHost_.setSelectedAssetIndex(selectedAssetIndex);
    if (selectionChanged || clipSettingsChanged) {
        // QtPropertiesPanel uses QSignalBlocker while it receives this state,
        // so an editor-to-view refresh never loops back as a user request.
        propertiesHost_.setSelectedAsset(asset.name, asset.kind, settings);
    }
    if (timelineViewChanged)
        timelineToolbarHost_.setViewState(timelineViewState);
#else
    if (selectionChanged) {
        mediaLibraryPane_.setSelectedAssetIndex(selectedAssetIndex);
        propertiesPane_.setSelectedAssetIndex(selectedAssetIndex);
    }
    if (selectionChanged || clipSettingsChanged)
        propertiesPane_.setClipSettings(settings);
#endif

    if (selectionChanged) {
        previewCanvas_.setSelectedAssetIndex(selectedAssetIndex);
#if MINI_EDITOR_USE_QT
        timelineCanvas_.setSelectedAssetIndex(selectedAssetIndex);
#else
        timelinePane_.setSelectedAssetIndex(selectedAssetIndex);
#endif
    }
    if (selectionChanged || clipSettingsChanged) {
        previewCanvas_.setClipSettings(settings);
#if MINI_EDITOR_USE_QT
        timelineCanvas_.setClipSettings(settings);
#else
        timelinePane_.setClipSettings(settings);
#endif
    }
    if (playbackChanged)
        previewCanvas_.setPlaybackState(playbackState);
#if MINI_EDITOR_USE_QT
    if (selectionChanged || timelineClipChanged)
        timelineCanvas_.setTimelineClipState(timelineClipState);
    if (timelineViewChanged)
        timelineCanvas_.setViewState(timelineViewState);
    if (playbackChanged) {
        timelineCanvas_.setPlaybackState(playbackState);
        transportHost_.setPlaybackState(playbackState);
    }
#else
    if (playbackChanged) {
        timelinePane_.setPlaybackState(playbackState);
        transportBar_.setPlaybackState(playbackState);
    }
#endif
    if (selectionChanged || clipSettingsChanged || playbackChanged)
        updateStatusText();
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
    const auto &asset = demoAssets()[editorSession_.selectedAssetIndex()];
    CString statusText;
    const ClipSettings &settings = editorSession_.selectedClipSettings();
    statusText.Format(_T("Selected: %s (%s) | Opacity %d%% | Scale %d%% | %s | Frame %d"),
                      asset.name, asset.kind, settings.opacityPercent,
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
    editorSession_.restoreWorkspaceState(settings->selectedAssetIndex,
                                         settings->timelineViewState);
}

void MainFrame::saveWorkspaceSettings() const
{
    WorkspaceSettings settings;
    const WorkspaceLayoutState layoutState = workspaceLayout_.state();
    settings.mediaLibraryWidth = layoutState.mediaLibraryWidth;
    settings.propertiesWidth = layoutState.propertiesWidth;
    settings.timelineHeight = layoutState.timelineHeight;
    settings.selectedAssetIndex = editorSession_.selectedAssetIndex();
    settings.timelineViewState = editorSession_.timelineViewState();
    WorkspaceSettingsStore::save(settings);
}

BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_COMMAND_RANGE(ID_MEDIA_ASSET_FIRST, ID_MEDIA_ASSET_LAST,
                     &MainFrame::OnSelectMediaAsset)
    ON_COMMAND_RANGE(ID_PLAYBACK_TOGGLE, ID_PLAYBACK_STEP_FORWARD,
                     &MainFrame::OnPlaybackCommand)
    ON_COMMAND(ID_EDIT_UNDO, &MainFrame::OnEditUndo)
    ON_COMMAND(ID_EDIT_REDO, &MainFrame::OnEditRedo)
    ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, &MainFrame::OnUpdateEditUndo)
    ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, &MainFrame::OnUpdateEditRedo)
    ON_WM_TIMER()
    ON_COMMAND(ID_FILE_EXIT, &MainFrame::OnFileExit)
END_MESSAGE_MAP()
