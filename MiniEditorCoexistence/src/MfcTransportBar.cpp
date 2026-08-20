#include "MfcTransportBar.h"

#include "resource.h"

namespace {

CString timecodeText(const PlaybackState &state)
{
    const int frames = state.currentFrame % state.framesPerSecond;
    const int totalSeconds = state.currentFrame / state.framesPerSecond;
    CString text;
    text.Format(_T("00:%02d:%02d:%02d"), totalSeconds % 60, frames);
    return text;
}

} // namespace

BEGIN_MESSAGE_MAP(MfcTransportBar, CWnd)
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

bool MfcTransportBar::Create(CWnd *parent, UINT controlId)
{
    const CString windowClass = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(::GetStockObject(NULL_BRUSH)), nullptr);
    return CWnd::CreateEx(0, windowClass, _T(""),
                           WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                           CRect(0, 0, 1, 1), parent, controlId) != FALSE;
}

void MfcTransportBar::setPlaybackState(const PlaybackState &state)
{
    playbackState_ = state;
    Invalidate();
}

void MfcTransportBar::OnPaint()
{
    CPaintDC deviceContext(this);
    CRect clientRect;
    GetClientRect(&clientRect);
    deviceContext.FillSolidRect(clientRect, RGB(27, 29, 34));
    deviceContext.Draw3dRect(clientRect, RGB(67, 70, 78), RGB(67, 70, 78));

    const CString labels[] = { _T("|<"), _T("<"),
                               playbackState_.isPlaying ? _T("Pause") : _T("Play"),
                               _T(">"), _T("Stop") };
    int left = 12;
    for (const CString &label : labels) {
        const CRect buttonRect(left, 7, left + 58, clientRect.bottom - 7);
        deviceContext.FillSolidRect(buttonRect, RGB(48, 52, 61));
        deviceContext.Draw3dRect(buttonRect, RGB(82, 88, 101), RGB(82, 88, 101));
        deviceContext.SetBkMode(TRANSPARENT);
        deviceContext.SetTextColor(RGB(225, 228, 235));
        deviceContext.DrawText(label, const_cast<CRect *>(&buttonRect),
                               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        left += 66;
    }

    const CRect timecodeRect(clientRect.right - 142, 7, clientRect.right - 12, clientRect.bottom - 7);
    deviceContext.FillSolidRect(timecodeRect, RGB(16, 17, 20));
    deviceContext.SetTextColor(RGB(230, 232, 237));
    deviceContext.DrawText(timecodeText(playbackState_), const_cast<CRect *>(&timecodeRect),
                           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void MfcTransportBar::OnLButtonDown(UINT flags, CPoint point)
{
    const int buttonIndex = (point.x - 12) / 66;
    if (buttonIndex >= 0 && buttonIndex < 5) {
        const UINT commands[] = {
            ID_PLAYBACK_STEP_BACKWARD,
            ID_PLAYBACK_STEP_BACKWARD,
            ID_PLAYBACK_TOGGLE,
            ID_PLAYBACK_STEP_FORWARD,
            ID_PLAYBACK_STOP
        };
        GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(commands[buttonIndex], 0));
    }

    CWnd::OnLButtonDown(flags, point);
}
