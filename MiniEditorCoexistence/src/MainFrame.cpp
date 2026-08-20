#include "MainFrame.h"

#include "DemoProject.h"
#include "resource.h"

#include <algorithm>

IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)

namespace {

constexpr UINT kStatusBarIndicators[] = { ID_SEPARATOR };
constexpr int kOuterMargin = 6;
constexpr int kMediaLibraryWidth = 304;
constexpr int kPropertiesWidth = 250;
constexpr int kTimelineHeight = 220;
constexpr int kTransportHeight = 42;
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
        || !timelinePane_.Create(this, IDC_TIMELINE)) {
        return -1;
    }

#if MINI_EDITOR_USE_QT
    if (!mediaLibraryHost_.create(GetSafeHwnd()))
        return -1;
    if (!propertiesHost_.create(GetSafeHwnd()))
        return -1;
    if (!transportHost_.create(GetSafeHwnd()))
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

    const int topAreaBottom = std::max(kOuterMargin,
                                       contentBottom - kTimelineHeight - kOuterMargin);
    const int centerLeft = kOuterMargin + kMediaLibraryWidth + kOuterMargin;
    const int centerRight = std::max(centerLeft + 120,
                                     clientWidth - kOuterMargin - kPropertiesWidth - kOuterMargin);
    const int canvasBottom = std::max(kOuterMargin,
                                      topAreaBottom - kTransportHeight - kOuterMargin);

#if MINI_EDITOR_USE_QT
    mediaLibraryHost_.resize(CRect(kOuterMargin, kOuterMargin,
                                   kOuterMargin + kMediaLibraryWidth,
                                   topAreaBottom));
#else
    mediaLibraryPane_.MoveWindow(kOuterMargin, kOuterMargin,
                                 kMediaLibraryWidth, topAreaBottom - kOuterMargin);
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
    propertiesHost_.resize(CRect(centerRight + kOuterMargin, kOuterMargin,
                                 clientWidth - kOuterMargin, topAreaBottom));
#else
    propertiesPane_.MoveWindow(centerRight + kOuterMargin, kOuterMargin,
                               std::max(0, clientWidth - centerRight - kOuterMargin * 2),
                               topAreaBottom - kOuterMargin);
#endif
    timelinePane_.MoveWindow(kOuterMargin, topAreaBottom + kOuterMargin,
                             std::max(0, clientWidth - kOuterMargin * 2),
                             std::max(0, contentBottom - topAreaBottom - kOuterMargin));
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
    timelinePane_.setSelectedAssetIndex(selectedAssetIndex_);
    timelinePane_.setClipSettings(settings);
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
    timelinePane_.setClipSettings(settings);
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

void MainFrame::synchronizePlaybackViews()
{
    previewCanvas_.setPlaybackState(playbackState_);
    timelinePane_.setPlaybackState(playbackState_);
#if MINI_EDITOR_USE_QT
    transportHost_.setPlaybackState(playbackState_);
#else
    transportBar_.setPlaybackState(playbackState_);
#endif
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
