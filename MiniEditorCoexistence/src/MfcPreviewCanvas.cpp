#include "MfcPreviewCanvas.h"

#include <algorithm>

namespace {

CString timecodeText(const PlaybackState &state)
{
    const int frames = state.currentFrame % state.framesPerSecond;
    const int totalSeconds = state.currentFrame / state.framesPerSecond;
    const int seconds = totalSeconds % 60;
    const int minutes = (totalSeconds / 60) % 60;
    const int hours = totalSeconds / 3600;

    CString text;
    text.Format(_T("%02d:%02d:%02d:%02d"), hours, minutes, seconds, frames);
    return text;
}

CString durationText(const PlaybackState &state)
{
    PlaybackState durationState = state;
    durationState.currentFrame = state.durationFrames;
    return timecodeText(durationState);
}

} // namespace

bool MfcPreviewCanvas::Create(CWnd *parent, UINT controlId)
{
    return createPane(parent, controlId);
}

void MfcPreviewCanvas::setPreviewState(const PreviewState &state)
{
    previewState_ = state;
    if (::IsWindow(GetSafeHwnd()))
        Invalidate(FALSE);
}

CString MfcPreviewCanvas::paneTitle() const
{
    return _T("Preview Canvas (MFC)");
}

void MfcPreviewCanvas::drawContent(CDC &deviceContext, const CRect &clientRect) const
{
    const ClipSettings &settings = previewState_.settings;
    const CRect availableRect(20, EditorUi::kHeaderHeight + 18,
                              clientRect.right - 20, clientRect.bottom - 12);
    const int availableWidth = availableRect.Width();
    const int availableHeight = availableRect.Height();
    const int baseVideoHeight = std::min(availableHeight, availableWidth * 9 / 16);
    const int videoHeight = std::max(1, std::min(availableHeight,
        baseVideoHeight * settings.scalePercent / 100));
    const int videoWidth = std::max(1, std::min(availableWidth, videoHeight * 16 / 9));

    int videoLeft = availableRect.left + (availableWidth - videoWidth) / 2;
    int videoTop = availableRect.top + (availableHeight - videoHeight) / 2;
    switch (settings.position) {
    case ClipPosition::TopLeft:
        videoLeft = availableRect.left;
        videoTop = availableRect.top;
        break;
    case ClipPosition::TopRight:
        videoLeft = availableRect.right - videoWidth;
        videoTop = availableRect.top;
        break;
    case ClipPosition::BottomLeft:
        videoLeft = availableRect.left;
        videoTop = availableRect.bottom - videoHeight;
        break;
    case ClipPosition::BottomRight:
        videoLeft = availableRect.right - videoWidth;
        videoTop = availableRect.bottom - videoHeight;
        break;
    case ClipPosition::Center:
        break;
    }

    const CRect videoRect(videoLeft, videoTop, videoLeft + videoWidth, videoTop + videoHeight);
    deviceContext.FillSolidRect(availableRect, EditorUi::kCanvasBackground);
    if (!previewState_.hasMedia) {
        drawText(deviceContext, _T("No media at this timeline position"), availableRect,
                 EditorUi::kSecondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    const BYTE red = static_cast<BYTE>((previewState_.thumbnailColorRgb >> 16) & 0xff);
    const BYTE green = static_cast<BYTE>((previewState_.thumbnailColorRgb >> 8) & 0xff);
    const BYTE blue = static_cast<BYTE>(previewState_.thumbnailColorRgb & 0xff);
    const COLORREF fadedThumbnailColor = RGB(red * settings.opacityPercent / 100,
                                             green * settings.opacityPercent / 100,
                                             blue * settings.opacityPercent / 100);
    deviceContext.FillSolidRect(videoRect, fadedThumbnailColor);
    deviceContext.Draw3dRect(videoRect, RGB(220, 220, 220), RGB(220, 220, 220));

    drawText(deviceContext, previewState_.displayName.c_str(),
             CRect(videoRect.left + 12, videoRect.bottom - 38,
                   videoRect.right - 12, videoRect.bottom - 12),
             RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    CString settingsText;
    settingsText.Format(_T("Opacity %d%%  |  Scale %d%%  |  %s"),
                        settings.opacityPercent, settings.scalePercent,
                        clipPositionDisplayName(settings.position));
    drawText(deviceContext, settingsText,
             CRect(availableRect.left, availableRect.bottom - 24,
                   availableRect.right, availableRect.bottom - 2),
             EditorUi::kSecondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    drawText(deviceContext, timecodeText(playbackState()),
             CRect(availableRect.right - 140, availableRect.top + 8,
                   availableRect.right - 8, availableRect.top + 30),
             RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (playbackState().isPlaying) {
        const CRect overlayRect(videoRect.left + (videoRect.Width() - 250) / 2,
                                videoRect.top + (videoRect.Height() - 54) / 2,
                                videoRect.left + (videoRect.Width() + 250) / 2,
                                videoRect.top + (videoRect.Height() + 54) / 2);
        deviceContext.FillSolidRect(overlayRect, RGB(18, 20, 24));
        deviceContext.Draw3dRect(overlayRect, RGB(150, 155, 165), RGB(150, 155, 165));

        CString overlayText;
        overlayText.Format(_T("%s / %s"), timecodeText(playbackState()),
                           durationText(playbackState()));
        drawText(deviceContext, overlayText, overlayRect, RGB(255, 255, 255),
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}
