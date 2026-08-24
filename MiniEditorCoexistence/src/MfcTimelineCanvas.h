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
    using TimelineClipEditedHandler = std::function<void(const TimelineClipState &state)>;

    bool Create(CWnd *parent, UINT controlId);
    void setSelectedAssetIndex(int selectedAssetIndex);
    void setClipSettings(const ClipSettings &settings);
    void setTimelineClipState(const TimelineClipState &state);
    void setPlaybackState(const PlaybackState &state);
    void setViewState(const TimelineViewState &state);
    void setSeekHandler(SeekHandler handler);
    void setTimelineClipEditedHandler(TimelineClipEditedHandler handler);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC *deviceContext);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnLButtonUp(UINT flags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    int frameAtRulerX(int x) const;
    int frameAtTimelineX(int x) const;
    CRect timelineClipRect() const;

    int selectedAssetIndex_ = 0;
    ClipSettings clipSettings_;
    TimelineClipState timelineClipState_;
    PlaybackState playbackState_;
    TimelineViewState viewState_;
    SeekHandler seekHandler_;
    TimelineClipEditedHandler timelineClipEditedHandler_;
    bool isDraggingClip_ = false;
    int dragFrameOffset_ = 0;
    TimelineClipState dragPreviewState_;
};
