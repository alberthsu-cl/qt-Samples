#pragma once

#include "ProjectState.h"

#include <afxwin.h>

// The custom-drawn timeline surface remains MFC during Phase 5. Qt replaces
// only the ordinary toolbar above it, a safer first migration seam.
class MfcTimelineCanvas final : public CWnd
{
public:
    bool Create(CWnd *parent, UINT controlId);
    void setSelectedAssetIndex(int selectedAssetIndex);
    void setClipSettings(const ClipSettings &settings);
    void setPlaybackState(const PlaybackState &state);
    void setViewState(const TimelineViewState &state);

protected:
    afx_msg void OnPaint();

    DECLARE_MESSAGE_MAP()

private:
    int selectedAssetIndex_ = 0;
    ClipSettings clipSettings_;
    PlaybackState playbackState_;
    TimelineViewState viewState_;
};
