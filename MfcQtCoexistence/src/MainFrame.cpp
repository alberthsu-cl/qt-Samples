#include "MainFrame.h"

#include "ImageProcessor.h"
#include "resource.h"

#if MFCQT_USE_QT
#include "QtEffectSettingsDialog.h"
#else
#include "EffectSettingsDialog.h"
#endif

#include <afxdlgs.h>

#include <algorithm>

IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)

namespace {

constexpr UINT kStatusBarIndicators[] = { ID_SEPARATOR };

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

    // One stretchable pane owns the complete status-bar message.
    statusBar_.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, 300);

#if MFCQT_USE_QT
    if (!qtPreviewHost_.create(GetSafeHwnd()))
        return -1;

    // Phase 3: the Qt preview is the source of a user toggle. Its signal is
    // delivered directly on this UI thread, then MFC stores the shared state.
    qtPreviewHost_.setDisplayModeChangedHandler([this](bool showingProcessedImage) {
        showProcessedImage_ = showingProcessedImage;
    });
#else
    if (!imageDisplay_.Create(this, CRect(0, 0, 1, 1)))
        return -1;

    if (!comparisonButton_.Create(this, CRect(0, 0, 1, 1), IDC_TOGGLE_IMAGE))
        return -1;
#endif

    if (!ImageProcessor::createDefaultImage(originalImage_))
        return -1;

    applySelectedEffect();
    CRect clientRect;
    GetClientRect(&clientRect);
    layoutChildren(clientRect.Width(), clientRect.Height());
    updateEffectStatus();
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

    // The image comparison sample needs enough room for its image region,
    // image-only toggle, and a readable status message.
    minMaxInfo->ptMinTrackSize.x = 560;
    minMaxInfo->ptMinTrackSize.y = 420;
}

void MainFrame::OnFileOpenImage()
{
    constexpr LPCTSTR filter =
        _T("Image files (*.png;*.jpg;*.jpeg;*.bmp)|*.png;*.jpg;*.jpeg;*.bmp|")
        _T("All files (*.*)|*.*||");

    CFileDialog openDialog(TRUE, nullptr, nullptr,
                           OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
                           filter, this);
    if (openDialog.DoModal() != IDOK)
        return;

    if (!ImageProcessor::loadImage(openDialog.GetPathName(), originalImage_)) {
        AfxMessageBox(_T("The selected image could not be loaded."));
        return;
    }

    showProcessedImage_ = true;
    applySelectedEffect();
    updateComparisonButton();
    updateEffectStatus();
}

void MainFrame::OnEffectSettings()
{
#if MFCQT_USE_QT
    // Qt does not know that this MFC frame is its parent window. Disable the
    // frame while the modal Qt dialog runs, then re-enable it afterward.
    EnableWindow(FALSE);
    QtEffectSettingsDialog dialog(effectSettings_);
    const bool accepted = dialog.exec() == QDialog::Accepted;
    EnableWindow(TRUE);
    SetActiveWindow();
#else
    EffectSettingsDialog dialog(effectSettings_, this);
    const bool accepted = dialog.DoModal() == IDOK;
#endif

    // Returning to the main window always restores the processed view. This
    // makes the selected effect the normal/default display after Settings is
    // closed, even if the user had been comparing the original image.
    showProcessedImage_ = true;

    if (accepted) {
        effectSettings_ = dialog.selectedSettings();
        applySelectedEffect();
        updateEffectStatus();
    } else {
#if MFCQT_USE_QT
        qtPreviewHost_.setShowingProcessedImage(true);
#else
        imageDisplay_.setShowProcessed(true);
#endif
    }

    updateComparisonButton();
}

void MainFrame::OnToggleImageComparison()
{
    showProcessedImage_ = !showProcessedImage_;
#if MFCQT_USE_QT
    qtPreviewHost_.setShowingProcessedImage(showProcessedImage_);
#else
    imageDisplay_.setShowProcessed(showProcessedImage_);
#endif
    updateComparisonButton();
}

void MainFrame::updateEffectStatus()
{
    CString statusText;
    // Keep the message compact enough to remain useful in a narrow window.
    statusText.Format(_T("Effect: %s"),
                      effectTypeDisplayName(effectSettings_.selectedEffect));
    statusBar_.SetPaneText(0, statusText);
}

void MainFrame::applySelectedEffect()
{
    if (!ImageProcessor::applyEffect(originalImage_,
                                     effectSettings_.selectedEffect,
                                     processedImage_)) {
        AfxMessageBox(_T("The effect could not be applied to this image."));
        return;
    }

#if MFCQT_USE_QT
    qtPreviewHost_.setImages(originalImage_, processedImage_);
    qtPreviewHost_.setShowingProcessedImage(showProcessedImage_);
#else
    imageDisplay_.setImages(&originalImage_, &processedImage_);
    imageDisplay_.setShowProcessed(showProcessedImage_);
#endif
}

void MainFrame::updateComparisonButton()
{
#if MFCQT_USE_QT
    qtPreviewHost_.setShowingProcessedImage(showProcessedImage_);
#else
    comparisonButton_.setShowingProcessedImage(showProcessedImage_);
#endif
}

void MainFrame::layoutChildren(int clientWidth, int clientHeight)
{
    constexpr int margin = 10;
    constexpr int buttonHeight = 30;
    constexpr int buttonWidth = 56;
    // Reserve the real CStatusBar rectangle before arranging manual child
    // windows. This prevents the display and image button from overlapping it.
    int contentBottom = clientHeight;
    if (::IsWindow(statusBar_.GetSafeHwnd())) {
        CRect statusBarRect;
        statusBar_.GetWindowRect(&statusBarRect);
        ScreenToClient(&statusBarRect);
        contentBottom = std::clamp(static_cast<int>(statusBarRect.top),
                                   0,
                                   clientHeight);
    }

    const int displayHeight = std::max(
        0, contentBottom - buttonHeight - margin * 3);

#if MFCQT_USE_QT
    qtPreviewHost_.resize(CRect(margin, margin,
                                std::max(margin, clientWidth - margin),
                                margin + displayHeight + buttonHeight + margin * 2));
#else
    if (!::IsWindow(imageDisplay_.GetSafeHwnd()))
        return;

    imageDisplay_.MoveWindow(margin, margin,
                             std::max(0, clientWidth - margin * 2),
                             displayHeight);

    if (::IsWindow(comparisonButton_.GetSafeHwnd())) {
        comparisonButton_.MoveWindow(std::max(margin, clientWidth - margin - buttonWidth),
                                     displayHeight + margin * 2,
                                     buttonWidth, buttonHeight);
    }
#endif
}

BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_COMMAND(ID_FILE_OPEN_IMAGE, &MainFrame::OnFileOpenImage)
    ON_COMMAND(ID_EFFECT_SETTINGS, &MainFrame::OnEffectSettings)
    ON_BN_CLICKED(IDC_TOGGLE_IMAGE, &MainFrame::OnToggleImageComparison)
END_MESSAGE_MAP()
