#pragma once

#if MINI_EDITOR_USE_QT_MEDIA_LIBRARY
#include "QtMediaLibraryHost.h"
#else
#include "MediaLibraryPane.h"
#endif
#include "PreviewPane.h"
#include "PropertiesPane.h"
#include "TimelinePane.h"

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
#if MINI_EDITOR_USE_QT_MEDIA_LIBRARY
    QtMediaLibraryHost mediaLibraryHost_;
#else
    MediaLibraryPane mediaLibraryPane_;
#endif
    PreviewPane previewPane_;
    PropertiesPane propertiesPane_;
    TimelinePane timelinePane_;
    int selectedAssetIndex_ = 0;
};
