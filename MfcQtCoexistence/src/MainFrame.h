#pragma once

#include "EffectSettings.h"
#include "ImageDisplayWindow.h"
#include "ImageToggleButton.h"
#if MFCQT_USE_QT
#include "QtPreviewHost.h"
#endif

#include <afxwin.h>
#include <afxext.h>
#include <afxcmn.h>

class MainFrame final : public CFrameWnd
{
    DECLARE_DYNCREATE(MainFrame)

public:
    ~MainFrame() override;

protected:
    int OnCreate(LPCREATESTRUCT createStructure);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO *minMaxInfo);
    afx_msg void OnFileOpenImage();
    afx_msg void OnEffectSettings();
    afx_msg void OnToggleImageComparison();

    DECLARE_MESSAGE_MAP()

private:
    void updateEffectStatus();
    void applySelectedEffect();
    void updateComparisonButton();
    void layoutChildren(int clientWidth, int clientHeight);

    CStatusBar statusBar_;
    ImageToggleButton comparisonButton_;
    ImageDisplayWindow imageDisplay_;
#if MFCQT_USE_QT
    QtPreviewHost qtPreviewHost_;
#endif
    EffectSettings effectSettings_;
    CImage originalImage_;
    CImage processedImage_;
    bool showProcessedImage_ = true;
};
