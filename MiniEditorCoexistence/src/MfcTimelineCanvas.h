#pragma once

#include "ProjectState.h"

#include <afxwin.h>

#include <functional>

// The custom-drawn timeline surface remains MFC during Phase 5. Qt replaces
// only the ordinary toolbar above it, a safer first migration seam.
class MfcTimelineCanvas final : public CWnd
{
public:
    using SeekHandler = std::function<void(int frame)>;

    bool Create(CWnd *parent, UINT controlId);
    void setSelectedAssetIndex(int selectedAssetIndex);
    void setClipSettings(const ClipSettings &settings);
    void setPlaybackState(const PlaybackState &state);
    void setViewState(const TimelineViewState &state);
    void setSeekHandler(SeekHandler handler);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC *deviceContext);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    int frameAtRulerX(int x) const;

    int selectedAssetIndex_ = 0;
    ClipSettings clipSettings_;
    PlaybackState playbackState_;
    TimelineViewState viewState_;
    SeekHandler seekHandler_;
};
