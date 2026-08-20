#pragma once

#include "ProjectState.h"

#if MINI_EDITOR_USE_QT
#include "QtMediaLibraryHost.h"
#include "QtPropertiesHost.h"
#else
#include "MediaLibraryPane.h"
#include "PropertiesPane.h"
#endif
#include "PreviewPane.h"
#include "TimelinePane.h"

#include <afxcmn.h>
#include <afxext.h>
#include <afxwin.h>

#include <array>

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
    void updateSelectedClipSettings(const ClipSettings &settings);
    void updateStatusText();

    CStatusBar statusBar_;
#if MINI_EDITOR_USE_QT
    QtMediaLibraryHost mediaLibraryHost_;
    QtPropertiesHost propertiesHost_;
#else
    MediaLibraryPane mediaLibraryPane_;
    PropertiesPane propertiesPane_;
#endif
    PreviewPane previewPane_;
    TimelinePane timelinePane_;
    int selectedAssetIndex_ = 0;
    std::array<ClipSettings, 6> clipSettings_;
};
