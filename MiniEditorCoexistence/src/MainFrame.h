#pragma once

#include "EditorPane.h"

#include <afxcmn.h>
#include <afxext.h>
#include <afxwin.h>

class MainFrame final : public CFrameWnd
{
    DECLARE_DYNCREATE(MainFrame)

public:
    ~MainFrame() override;

protected:
    afx_msg int OnCreate(LPCREATESTRUCT createStructure);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO *minMaxInfo);
    afx_msg void OnSelectMediaAsset(UINT commandId);
    afx_msg void OnFileExit();

    DECLARE_MESSAGE_MAP()

private:
    void layoutChildren(int clientWidth, int clientHeight);
    void selectAsset(int assetIndex);
    void updateStatusText();

    CStatusBar statusBar_;
    EditorPane mediaLibraryPane_;
    EditorPane previewPane_;
    EditorPane propertiesPane_;
    EditorPane timelinePane_;
    int selectedAssetIndex_ = 0;
};
