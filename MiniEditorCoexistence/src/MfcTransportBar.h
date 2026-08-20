#pragma once

#include "ProjectState.h"

#include <afxwin.h>

// Pure-MFC transport baseline. In the Qt build, QtTransportHost replaces only
// this child region while MfcPreviewCanvas remains untouched.
class MfcTransportBar final : public CWnd
{
public:
    bool Create(CWnd *parent, UINT controlId);
    void setPlaybackState(const PlaybackState &state);

protected:
    afx_msg void OnPaint();
    afx_msg void OnLButtonDown(UINT flags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    PlaybackState playbackState_;
};
