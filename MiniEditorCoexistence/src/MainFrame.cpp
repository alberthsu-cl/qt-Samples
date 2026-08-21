#include "MainFrame.h"

#include "DemoProject.h"
#include "resource.h"

#include <algorithm>

IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)

namespace {

constexpr UINT kStatusBarIndicators[] = { ID_SEPARATOR };
constexpr int kOuterMargin = 6;
constexpr int kTransportHeight = 42;
constexpr int kTimelineToolbarHeight = 44;
constexpr int kSplitterThickness = 6;
constexpr int kMinimumMediaLibraryWidth = 180;
constexpr int kMinimumPropertiesWidth = 190;
constexpr int kMinimumPreviewWidth = 320;
constexpr int kMinimumTopAreaHeight = 250;
constexpr int kMinimumTimelineHeight = 140;
constexpr UINT_PTR kPlaybackTimerId = 1;

} // namespace

MainFrame::~MainFrame()
{
    if (::IsWindow(GetSafeHwnd()))
        KillTimer(kPlaybackTimerId);
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
        selectAsset(assetIndex);
    });
    propertiesHost_.setClipSettingsEditedHandler([this](const ClipSettings &settings) {
        updateSelectedClipSettings(settings);
    });
    transportHost_.setPlaybackCommandHandler([this](PlaybackCommand command) {
        handlePlaybackCommand(command);
    });
    timelineToolbarHost_.setViewStateEditedHandler([this](const TimelineViewState &state) {
        updateTimelineViewState(state);
    });
    timelineToolbarHost_.setFitTimelineHandler([this] { fitTimeline(); });
#else
    if (!mediaLibraryPane_.Create(this, IDC_MEDIA_LIBRARY)
        || !propertiesPane_.Create(this, IDC_PROPERTIES)
        || !transportBar_.Create(this, IDC_TRANSPORT)) {
        return -1;
    }
#endif

    selectAsset(0);
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
    selectAsset(static_cast<int>(commandId - ID_MEDIA_ASSET_FIRST));
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

void MainFrame::OnTimer(UINT_PTR timerId)
{
    if (timerId == kPlaybackTimerId && playbackState_.isPlaying) {
        // This is a deliberately simple MFC timer. In a production editor the
        // media engine would report its clock/playhead instead.
        playbackState_.currentFrame = (playbackState_.currentFrame + 1) % 300;
        synchronizePlaybackViews();
        updateStatusText();
        return;
    }

    CFrameWnd::OnTimer(timerId);
}

void MainFrame::OnFileExit()
{
    SendMessage(WM_CLOSE);
}

void MainFrame::layoutChildren(int clientWidth, int clientHeight)
{
#if !MINI_EDITOR_USE_QT
    if (!::IsWindow(mediaLibraryPane_.GetSafeHwnd())
        || !::IsWindow(propertiesPane_.GetSafeHwnd()))
        return;
#endif

    int contentBottom = clientHeight;
    if (::IsWindow(statusBar_.GetSafeHwnd())) {
        CRect statusRect;
        statusBar_.GetWindowRect(&statusRect);
        ScreenToClient(&statusRect);
        contentBottom = std::clamp(static_cast<int>(statusRect.top), 0, clientHeight);
    }

    const int maximumTimelineHeight = std::max(kMinimumTimelineHeight,
        contentBottom - kOuterMargin - kSplitterThickness - kMinimumTopAreaHeight);
    timelineHeight_ = std::clamp(timelineHeight_, kMinimumTimelineHeight,
                                 maximumTimelineHeight);
    const int timelineTop = contentBottom - timelineHeight_;
    const int timelineSplitterTop = timelineTop - kSplitterThickness;

    const int maximumMediaWidth = std::max(kMinimumMediaLibraryWidth,
        clientWidth - kOuterMargin * 2 - kSplitterThickness * 2
                    - kMinimumPropertiesWidth - kMinimumPreviewWidth);
    mediaLibraryWidth_ = std::clamp(mediaLibraryWidth_, kMinimumMediaLibraryWidth,
                                    maximumMediaWidth);
    const int maximumPropertiesWidth = std::max(kMinimumPropertiesWidth,
        clientWidth - kOuterMargin * 2 - kSplitterThickness * 2
                    - mediaLibraryWidth_ - kMinimumPreviewWidth);
    propertiesWidth_ = std::clamp(propertiesWidth_, kMinimumPropertiesWidth,
                                  maximumPropertiesWidth);

    const int mediaRight = kOuterMargin + mediaLibraryWidth_;
    const int centerLeft = mediaRight + kSplitterThickness;
    const int propertiesLeft = clientWidth - kOuterMargin - propertiesWidth_;
    const int rightSplitterLeft = propertiesLeft - kSplitterThickness;
    const int centerRight = rightSplitterLeft;
    const int topAreaBottom = timelineSplitterTop;
    const int canvasBottom = std::max(kOuterMargin,
                                      topAreaBottom - kTransportHeight - kOuterMargin);

#if MINI_EDITOR_USE_QT
    mediaLibraryHost_.resize(CRect(kOuterMargin, kOuterMargin,
                                   mediaRight,
                                   topAreaBottom));
#else
    mediaLibraryPane_.MoveWindow(kOuterMargin, kOuterMargin,
                                 mediaLibraryWidth_, topAreaBottom - kOuterMargin);
#endif
    previewCanvas_.MoveWindow(centerLeft, kOuterMargin,
                              centerRight - centerLeft, canvasBottom - kOuterMargin);
#if MINI_EDITOR_USE_QT
    transportHost_.resize(CRect(centerLeft, canvasBottom + kOuterMargin,
                                centerRight, topAreaBottom));
#else
    transportBar_.MoveWindow(centerLeft, canvasBottom + kOuterMargin,
                             centerRight - centerLeft, kTransportHeight);
#endif
#if MINI_EDITOR_USE_QT
    propertiesHost_.resize(CRect(propertiesLeft, kOuterMargin,
                                 clientWidth - kOuterMargin, topAreaBottom));
#else
    propertiesPane_.MoveWindow(propertiesLeft, kOuterMargin,
                               propertiesWidth_,
                               topAreaBottom - kOuterMargin);
#endif
#if MINI_EDITOR_USE_QT
    const int timelineWidth = std::max(0, clientWidth - kOuterMargin * 2);
    timelineToolbarHost_.resize(CRect(kOuterMargin, timelineTop,
                                      clientWidth - kOuterMargin,
                                      timelineTop + kTimelineToolbarHeight));
    timelineCanvas_.MoveWindow(kOuterMargin, timelineTop + kTimelineToolbarHeight,
                               timelineWidth,
                               std::max(0, timelineHeight_ - kTimelineToolbarHeight));
#else
    timelinePane_.MoveWindow(kOuterMargin, timelineTop,
                             std::max(0, clientWidth - kOuterMargin * 2),
                             timelineHeight_);
#endif

    leftSplitter_.MoveWindow(mediaRight, kOuterMargin,
                             kSplitterThickness, topAreaBottom - kOuterMargin);
    rightSplitter_.MoveWindow(rightSplitterLeft, kOuterMargin,
                              kSplitterThickness, topAreaBottom - kOuterMargin);
    timelineSplitter_.MoveWindow(kOuterMargin, timelineSplitterTop,
                                 std::max(0, clientWidth - kOuterMargin * 2),
                                 kSplitterThickness);
}

void MainFrame::selectAsset(int assetIndex)
{
    selectedAssetIndex_ = std::clamp(assetIndex, 0,
                                     static_cast<int>(demoAssets().size()) - 1);
    const auto &asset = demoAssets()[selectedAssetIndex_];
    const ClipSettings &settings = clipSettings_[selectedAssetIndex_];

#if MINI_EDITOR_USE_QT
    mediaLibraryHost_.setSelectedAssetIndex(selectedAssetIndex_);
    propertiesHost_.setSelectedAsset(asset.name, asset.kind, settings);
#else
    mediaLibraryPane_.setSelectedAssetIndex(selectedAssetIndex_);
    propertiesPane_.setSelectedAssetIndex(selectedAssetIndex_);
    propertiesPane_.setClipSettings(settings);
#endif
    previewCanvas_.setSelectedAssetIndex(selectedAssetIndex_);
    previewCanvas_.setClipSettings(settings);
#if MINI_EDITOR_USE_QT
    timelineCanvas_.setSelectedAssetIndex(selectedAssetIndex_);
    timelineCanvas_.setClipSettings(settings);
    timelineCanvas_.setViewState(timelineViewState_);
    timelineToolbarHost_.setViewState(timelineViewState_);
#else
    timelinePane_.setSelectedAssetIndex(selectedAssetIndex_);
    timelinePane_.setClipSettings(settings);
#endif
    synchronizePlaybackViews();
    updateStatusText();
}

void MainFrame::updateSelectedClipSettings(const ClipSettings &settings)
{
    clipSettings_[selectedAssetIndex_] = settings;
    const auto &asset = demoAssets()[selectedAssetIndex_];

#if MINI_EDITOR_USE_QT
    // This synchronization is signal-blocked inside QtPropertiesPanel, so a
    // user edit never feeds back as another user edit.
    propertiesHost_.setSelectedAsset(asset.name, asset.kind, settings);
#else
    propertiesPane_.setClipSettings(settings);
#endif
    previewCanvas_.setClipSettings(settings);
#if MINI_EDITOR_USE_QT
    timelineCanvas_.setClipSettings(settings);
#else
    timelinePane_.setClipSettings(settings);
#endif
    updateStatusText();
}

void MainFrame::handlePlaybackCommand(PlaybackCommand command)
{
    switch (command) {
    case PlaybackCommand::TogglePlayPause:
        playbackState_.isPlaying = !playbackState_.isPlaying;
        if (playbackState_.isPlaying)
            SetTimer(kPlaybackTimerId, 33, nullptr);
        else
            KillTimer(kPlaybackTimerId);
        break;
    case PlaybackCommand::Stop:
        playbackState_.isPlaying = false;
        playbackState_.currentFrame = 0;
        KillTimer(kPlaybackTimerId);
        break;
    case PlaybackCommand::StepBackward:
        playbackState_.isPlaying = false;
        playbackState_.currentFrame = std::max(0, playbackState_.currentFrame - 1);
        KillTimer(kPlaybackTimerId);
        break;
    case PlaybackCommand::StepForward:
        playbackState_.isPlaying = false;
        playbackState_.currentFrame = std::min(299, playbackState_.currentFrame + 1);
        KillTimer(kPlaybackTimerId);
        break;
    }

    synchronizePlaybackViews();
    updateStatusText();
}

void MainFrame::updateTimelineViewState(const TimelineViewState &state)
{
    timelineViewState_ = state;
#if MINI_EDITOR_USE_QT
    timelineCanvas_.setViewState(timelineViewState_);
    timelineToolbarHost_.setViewState(timelineViewState_);
#endif
}

void MainFrame::fitTimeline()
{
    timelineViewState_.zoomPercent = 100;
    updateTimelineViewState(timelineViewState_);
}

void MainFrame::synchronizePlaybackViews()
{
    previewCanvas_.setPlaybackState(playbackState_);
#if MINI_EDITOR_USE_QT
    timelineCanvas_.setPlaybackState(playbackState_);
#else
    timelinePane_.setPlaybackState(playbackState_);
#endif
#if MINI_EDITOR_USE_QT
    transportHost_.setPlaybackState(playbackState_);
#else
    transportBar_.setPlaybackState(playbackState_);
#endif
}

void MainFrame::moveLeftSplitter(int parentX)
{
    mediaLibraryWidth_ = std::max(kMinimumMediaLibraryWidth, parentX - kOuterMargin);
    CRect clientRect;
    GetClientRect(&clientRect);
    layoutChildren(clientRect.Width(), clientRect.Height());
}

void MainFrame::moveRightSplitter(int parentX)
{
    CRect clientRect;
    GetClientRect(&clientRect);
    propertiesWidth_ = std::max(kMinimumPropertiesWidth,
        clientRect.Width() - kOuterMargin - (parentX + kSplitterThickness));
    layoutChildren(clientRect.Width(), clientRect.Height());
}

void MainFrame::moveTimelineSplitter(int parentY)
{
    CRect clientRect;
    GetClientRect(&clientRect);

    int contentBottom = clientRect.Height();
    if (::IsWindow(statusBar_.GetSafeHwnd())) {
        CRect statusRect;
        statusBar_.GetWindowRect(&statusRect);
        ScreenToClient(&statusRect);
        contentBottom = std::clamp(static_cast<int>(statusRect.top), 0, clientRect.Height());
    }

    timelineHeight_ = std::max(kMinimumTimelineHeight,
        contentBottom - (parentY + kSplitterThickness));
    layoutChildren(clientRect.Width(), clientRect.Height());
}

void MainFrame::updateStatusText()
{
    const auto &asset = demoAssets()[selectedAssetIndex_];
    CString statusText;
    const ClipSettings &settings = clipSettings_[selectedAssetIndex_];
    statusText.Format(_T("Selected: %s (%s) | Opacity %d%% | Scale %d%% | %s"),
                      asset.name, asset.kind, settings.opacityPercent,
                      settings.scalePercent, clipPositionDisplayName(settings.position));
    statusBar_.SetPaneText(0, statusText);
}

BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_COMMAND_RANGE(ID_MEDIA_ASSET_FIRST, ID_MEDIA_ASSET_LAST,
                     &MainFrame::OnSelectMediaAsset)
    ON_COMMAND_RANGE(ID_PLAYBACK_TOGGLE, ID_PLAYBACK_STEP_FORWARD,
                     &MainFrame::OnPlaybackCommand)
    ON_WM_TIMER()
    ON_COMMAND(ID_FILE_EXIT, &MainFrame::OnFileExit)
END_MESSAGE_MAP()
