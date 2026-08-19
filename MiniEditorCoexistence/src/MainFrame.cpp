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

} // namespace

MainFrame::~MainFrame() = default;

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

    if (!previewPane_.Create(this, IDC_PREVIEW)
        || !propertiesPane_.Create(this, IDC_PROPERTIES)
        || !timelinePane_.Create(this, IDC_TIMELINE)) {
        return -1;
    }

#if MINI_EDITOR_USE_QT_MEDIA_LIBRARY
    if (!mediaLibraryHost_.create(GetSafeHwnd()))
        return -1;

    // The Qt panel emits a framework-neutral asset index. MFC continues to
    // own project selection and updates the three remaining MFC panes.
    mediaLibraryHost_.setAssetSelectedHandler([this](int assetIndex) {
        selectAsset(assetIndex);
    });
#else
    if (!mediaLibraryPane_.Create(this, IDC_MEDIA_LIBRARY))
        return -1;
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

void MainFrame::OnFileExit()
{
    SendMessage(WM_CLOSE);
}

void MainFrame::layoutChildren(int clientWidth, int clientHeight)
{
#if !MINI_EDITOR_USE_QT_MEDIA_LIBRARY
    if (!::IsWindow(mediaLibraryPane_.GetSafeHwnd()))
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

#if MINI_EDITOR_USE_QT_MEDIA_LIBRARY
    mediaLibraryHost_.resize(CRect(kOuterMargin, kOuterMargin,
                                   kOuterMargin + kMediaLibraryWidth,
                                   topAreaBottom));
#else
    mediaLibraryPane_.MoveWindow(kOuterMargin, kOuterMargin,
                                 kMediaLibraryWidth, topAreaBottom - kOuterMargin);
#endif
    previewPane_.MoveWindow(centerLeft, kOuterMargin,
                            centerRight - centerLeft, topAreaBottom - kOuterMargin);
    propertiesPane_.MoveWindow(centerRight + kOuterMargin, kOuterMargin,
                               std::max(0, clientWidth - centerRight - kOuterMargin * 2),
                               topAreaBottom - kOuterMargin);
    timelinePane_.MoveWindow(kOuterMargin, topAreaBottom + kOuterMargin,
                             std::max(0, clientWidth - kOuterMargin * 2),
                             std::max(0, contentBottom - topAreaBottom - kOuterMargin));
}

void MainFrame::selectAsset(int assetIndex)
{
    selectedAssetIndex_ = std::clamp(assetIndex, 0,
                                     static_cast<int>(demoAssets().size()) - 1);
#if MINI_EDITOR_USE_QT_MEDIA_LIBRARY
    mediaLibraryHost_.setSelectedAssetIndex(selectedAssetIndex_);
#else
    mediaLibraryPane_.setSelectedAssetIndex(selectedAssetIndex_);
#endif
    previewPane_.setSelectedAssetIndex(selectedAssetIndex_);
    propertiesPane_.setSelectedAssetIndex(selectedAssetIndex_);
    timelinePane_.setSelectedAssetIndex(selectedAssetIndex_);
    updateStatusText();
}

void MainFrame::updateStatusText()
{
    const auto &asset = demoAssets()[selectedAssetIndex_];
    CString statusText;
    statusText.Format(_T("Phase 0 — Pure MFC baseline | Selected: %s (%s)"),
                      asset.name, asset.kind);
    statusBar_.SetPaneText(0, statusText);
}

BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_COMMAND_RANGE(ID_MEDIA_ASSET_FIRST, ID_MEDIA_ASSET_LAST,
                     &MainFrame::OnSelectMediaAsset)
    ON_COMMAND(ID_FILE_EXIT, &MainFrame::OnFileExit)
END_MESSAGE_MAP()
