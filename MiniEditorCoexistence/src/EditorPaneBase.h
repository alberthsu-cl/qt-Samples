#pragma once

#include "ProjectState.h"

#include <afxwin.h>

// Shared MFC mechanics only. Each editor area provides its own title, drawing,
// and input behavior through a focused derived class.
class EditorPaneBase : public CWnd
{
public:
    void setSelectedAssetIndex(int selectedAssetIndex);
    void setClipSettings(const ClipSettings &settings);
    void setPlaybackState(const PlaybackState &state);

protected:
    bool createPane(CWnd *parent, UINT controlId);
    int selectedAssetIndex() const;
    const ClipSettings &clipSettings() const;
    const PlaybackState &playbackState() const;

    void drawPaneTitle(CDC &deviceContext, const CString &title) const;
    void drawText(CDC &deviceContext, const CString &text, const CRect &bounds,
                  COLORREF color, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) const;

    virtual CString paneTitle() const = 0;
    virtual void drawContent(CDC &deviceContext, const CRect &clientRect) const = 0;

    afx_msg void OnPaint();

    DECLARE_MESSAGE_MAP()

private:
    int selectedAssetIndex_ = 0;
    ClipSettings clipSettings_;
    PlaybackState playbackState_;
};

namespace EditorUi {

inline constexpr COLORREF kPanelBackground = RGB(35, 37, 43);
inline constexpr COLORREF kPanelBorder = RGB(67, 70, 78);
inline constexpr COLORREF kCanvasBackground = RGB(18, 19, 22);
inline constexpr COLORREF kText = RGB(230, 232, 237);
inline constexpr COLORREF kSecondaryText = RGB(166, 171, 183);
inline constexpr COLORREF kAccent = RGB(42, 136, 235);
inline constexpr int kHeaderHeight = 42;

} // namespace EditorUi
