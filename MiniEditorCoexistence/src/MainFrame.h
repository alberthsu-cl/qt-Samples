#pragma once

#include "ProjectState.h"

#if MINI_EDITOR_USE_QT
#include "QtMediaLibraryHost.h"
#include "QtPropertiesHost.h"
#include "QtTransportHost.h"
#else
#include "MfcMediaLibraryPane.h"
#include "MfcTransportBar.h"
#include "MfcPropertiesPane.h"
#endif
#include "MfcPreviewCanvas.h"
#include "MfcTimelinePane.h"
#include "MfcWorkspaceSplitter.h"

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
    afx_msg void OnPlaybackCommand(UINT commandId);
    afx_msg void OnTimer(UINT_PTR timerId);
    afx_msg void OnFileExit();

    DECLARE_MESSAGE_MAP()

private:
    void layoutChildren(int clientWidth, int clientHeight);
    void selectAsset(int assetIndex);
    void updateSelectedClipSettings(const ClipSettings &settings);
    void handlePlaybackCommand(PlaybackCommand command);
    void synchronizePlaybackViews();
    void moveLeftSplitter(int parentX);
    void moveRightSplitter(int parentX);
    void moveTimelineSplitter(int parentY);
    void updateStatusText();

    CStatusBar statusBar_;
#if MINI_EDITOR_USE_QT
    QtMediaLibraryHost mediaLibraryHost_;
    QtPropertiesHost propertiesHost_;
    QtTransportHost transportHost_;
#else
    MfcMediaLibraryPane mediaLibraryPane_;
    MfcPropertiesPane propertiesPane_;
    MfcTransportBar transportBar_;
#endif
    MfcPreviewCanvas previewCanvas_;
    MfcTimelinePane timelinePane_;
    MfcWorkspaceSplitter leftSplitter_;
    MfcWorkspaceSplitter rightSplitter_;
    MfcWorkspaceSplitter timelineSplitter_;
    int selectedAssetIndex_ = 0;
    std::array<ClipSettings, 6> clipSettings_;
    PlaybackState playbackState_;
    int mediaLibraryWidth_ = 304;
    int propertiesWidth_ = 250;
    int timelineHeight_ = 220;
};
