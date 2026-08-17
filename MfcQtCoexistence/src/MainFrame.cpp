#include "MainFrame.h"

#include "EffectSettingsDialog.h"
#include "ImageProcessor.h"
#include "resource.h"

#include <afxdlgs.h>

#include <algorithm>

IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)

namespace {

constexpr UINT kStatusBarIndicators[] = { ID_SEPARATOR };

} // namespace

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

    if (!imageDisplay_.Create(this, CRect(0, 0, 1, 1)))
        return -1;

    if (!comparisonButton_.Create(this, CRect(0, 0, 1, 1), IDC_TOGGLE_IMAGE)) {
        return -1;
    }

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
    EffectSettingsDialog dialog(effectSettings_, this);
    const INT_PTR dialogResult = dialog.DoModal();

    // Returning to the main window always restores the processed view. This
    // makes the selected effect the normal/default display after Settings is
    // closed, even if the user had been comparing the original image.
    showProcessedImage_ = true;

    if (dialogResult == IDOK) {
        effectSettings_ = dialog.selectedSettings();
        applySelectedEffect();
        updateEffectStatus();
    } else {
        imageDisplay_.setShowProcessed(true);
    }

    updateComparisonButton();
}

void MainFrame::OnToggleImageComparison()
{
    showProcessedImage_ = !showProcessedImage_;
    imageDisplay_.setShowProcessed(showProcessedImage_);
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

    imageDisplay_.setImages(&originalImage_, &processedImage_);
    imageDisplay_.setShowProcessed(showProcessedImage_);
}

void MainFrame::updateComparisonButton()
{
    comparisonButton_.setShowingProcessedImage(showProcessedImage_);
}

void MainFrame::layoutChildren(int clientWidth, int clientHeight)
{
    if (!::IsWindow(imageDisplay_.GetSafeHwnd()))
        return;

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

    imageDisplay_.MoveWindow(margin, margin,
                             std::max(0, clientWidth - margin * 2),
                             displayHeight);

    if (::IsWindow(comparisonButton_.GetSafeHwnd())) {
        comparisonButton_.MoveWindow(std::max(margin, clientWidth - margin - buttonWidth),
                                     displayHeight + margin * 2,
                                     buttonWidth, buttonHeight);
    }
}

BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_COMMAND(ID_FILE_OPEN_IMAGE, &MainFrame::OnFileOpenImage)
    ON_COMMAND(ID_EFFECT_SETTINGS, &MainFrame::OnEffectSettings)
    ON_BN_CLICKED(IDC_TOGGLE_IMAGE, &MainFrame::OnToggleImageComparison)
END_MESSAGE_MAP()
