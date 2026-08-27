#include "MfcPreviewCanvas.h"

#include <algorithm>

namespace {

CString frameTimecode(int frame, int framesPerSecond)
{
    frame = std::max(0, frame);
    framesPerSecond = std::max(1, framesPerSecond);
    const int frames = frame % framesPerSecond;
    const int totalSeconds = frame / framesPerSecond;
    const int seconds = totalSeconds % 60;
    const int minutes = (totalSeconds / 60) % 60;
    const int hours = totalSeconds / 3600;

    CString text;
    text.Format(_T("%02d:%02d:%02d:%02d"), hours, minutes, seconds, frames);
    return text;
}

CString timecodeText(const PlaybackState &state)
{
    return frameTimecode(state.currentFrame, state.framesPerSecond);
}

CString durationText(const PlaybackState &state)
{
    return frameTimecode(state.durationFrames, state.framesPerSecond);
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
    } else {
        const BYTE red = static_cast<BYTE>((previewState_.thumbnailColorRgb >> 16) & 0xff);
        const BYTE green = static_cast<BYTE>((previewState_.thumbnailColorRgb >> 8) & 0xff);
        const BYTE blue = static_cast<BYTE>(previewState_.thumbnailColorRgb & 0xff);
        // PreviewStateResolver already folded the clip's fade ramp into this
        // value, so the MFC renderer matches the Qt renderer exactly.
        const int renderedOpacityPercent = previewState_.effectiveOpacityPercent;
        const COLORREF fadedThumbnailColor = RGB(red * renderedOpacityPercent / 100,
                                                 green * renderedOpacityPercent / 100,
                                                 blue * renderedOpacityPercent / 100);
        deviceContext.FillSolidRect(videoRect, fadedThumbnailColor);
        deviceContext.Draw3dRect(videoRect, RGB(220, 220, 220), RGB(220, 220, 220));

        drawText(deviceContext, previewState_.displayName.c_str(),
                 CRect(videoRect.left + 12, videoRect.bottom - 38,
                       videoRect.right - 12, videoRect.bottom - 12),
                 RGB(255, 255, 255),
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        CString settingsText;
        settingsText.Format(_T("Opacity %d%%  |  Scale %d%%  |  %s"),
                            settings.opacityPercent, settings.scalePercent,
                            clipPositionDisplayName(settings.position));
        drawText(deviceContext, settingsText,
                 CRect(availableRect.left, availableRect.bottom - 24,
                       availableRect.right, availableRect.bottom - 2),
                 EditorUi::kSecondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    drawText(deviceContext, timecodeText(playbackState()),
             CRect(availableRect.right - 140, availableRect.top + 8,
                   availableRect.right - 8, availableRect.top + 30),
             RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // A paused preview still represents a resolved media frame. Keep its
    // diagnostics visible so timeline/source mapping can be inspected; only
    // an explicit Stop hides the overlay.
    if (playbackState().isPlaying || playbackState().isPaused) {
        const CRect overlayBase = previewState_.hasMedia ? videoRect : availableRect;
        const int overlayWidth = std::min(480, std::max(220, overlayBase.Width() - 24));
        const int overlayHeight = previewState_.hasAudio ? 96 : 74;
        const CRect overlayRect(
            overlayBase.left + (overlayBase.Width() - overlayWidth) / 2,
            overlayBase.top + (overlayBase.Height() - overlayHeight) / 2,
            overlayBase.left + (overlayBase.Width() + overlayWidth) / 2,
            overlayBase.top + (overlayBase.Height() + overlayHeight) / 2);
        deviceContext.FillSolidRect(overlayRect, RGB(18, 20, 24));
        deviceContext.Draw3dRect(overlayRect, RGB(150, 155, 165), RGB(150, 155, 165));

        CString overlayText;
        if (previewState_.mode == PreviewMode::Source) {
            if (previewState_.mediaKind == MediaKind::Image) {
                overlayText.Format(_T("Source preview\nImage display frame %d"),
                                   playbackState().currentFrame);
            } else {
                overlayText.Format(
                    _T("Source %s / %s"),
                    frameTimecode(previewState_.sourceFrame,
                                  playbackState().framesPerSecond),
                    frameTimecode(previewState_.sourceDurationFrames,
                                  playbackState().framesPerSecond));
            }
        } else {
            overlayText.Format(_T("Timeline %s / %s"),
                               timecodeText(playbackState()),
                               durationText(playbackState()));
            if (previewState_.hasMedia) {
                CString videoText;
                if (previewState_.mediaKind == MediaKind::Image) {
                    videoText.Format(_T("\nImage display frame %d"),
                                     previewState_.clipLocalFrame);
                } else {
                    videoText.Format(
                        _T("\nVideo source %s / %s"),
                        frameTimecode(previewState_.sourceFrame,
                                      playbackState().framesPerSecond),
                        frameTimecode(previewState_.sourceDurationFrames,
                                      playbackState().framesPerSecond));
                }
                overlayText += videoText;
            } else {
                overlayText += _T("\nNo video at this position");
            }
            if (previewState_.hasAudio) {
                CString audioText;
                audioText.Format(
                    _T("\nAudio source %s / %s"),
                    frameTimecode(previewState_.audioSourceFrame,
                                  playbackState().framesPerSecond),
                    frameTimecode(previewState_.audioSourceDurationFrames,
                                  playbackState().framesPerSecond));
                overlayText += audioText;
            }
        }
        drawText(deviceContext, overlayText, overlayRect, RGB(255, 255, 255),
                 DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    }
}
