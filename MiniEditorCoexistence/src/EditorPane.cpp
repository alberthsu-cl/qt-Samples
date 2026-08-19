#include "EditorPane.h"

#include "resource.h"

#include <algorithm>

namespace {

constexpr COLORREF kPanelBackground = RGB(35, 37, 43);
constexpr COLORREF kPanelBorder = RGB(67, 70, 78);
constexpr COLORREF kCanvasBackground = RGB(18, 19, 22);
constexpr COLORREF kText = RGB(230, 232, 237);
constexpr COLORREF kSecondaryText = RGB(166, 171, 183);
constexpr COLORREF kAccent = RGB(42, 136, 235);

constexpr int kHeaderHeight = 42;
constexpr int kAssetWidth = 132;
constexpr int kAssetHeight = 112;
constexpr int kAssetSpacing = 12;

} // namespace

BEGIN_MESSAGE_MAP(EditorPane, CWnd)
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

bool EditorPane::Create(EditorPaneKind kind, CWnd *parent, UINT controlId)
{
    kind_ = kind;

    const CString windowClass = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(::GetStockObject(NULL_BRUSH)),
        nullptr);

    return CWnd::CreateEx(0, windowClass, _T(""),
                           WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                           CRect(0, 0, 1, 1), parent, controlId) != FALSE;
}

void EditorPane::setSelectedAssetIndex(int selectedAssetIndex)
{
    selectedAssetIndex_ = std::clamp(selectedAssetIndex, 0,
                                     static_cast<int>(demoAssets().size()) - 1);
    Invalidate();
}

void EditorPane::OnPaint()
{
    CPaintDC deviceContext(this);
    CRect clientRect;
    GetClientRect(&clientRect);

    deviceContext.FillSolidRect(clientRect, kPanelBackground);
    deviceContext.Draw3dRect(clientRect, kPanelBorder, kPanelBorder);

    switch (kind_) {
    case EditorPaneKind::MediaLibrary:
        drawMediaLibrary(deviceContext, clientRect);
        break;
    case EditorPaneKind::Preview:
        drawPreview(deviceContext, clientRect);
        break;
    case EditorPaneKind::Properties:
        drawProperties(deviceContext, clientRect);
        break;
    case EditorPaneKind::Timeline:
        drawTimeline(deviceContext, clientRect);
        break;
    }
}

void EditorPane::OnLButtonDown(UINT flags, CPoint point)
{
    if (kind_ == EditorPaneKind::MediaLibrary) {
        const int assetIndex = mediaAssetAt(point);
        if (assetIndex >= 0) {
            GetParent()->SendMessage(WM_COMMAND,
                                     MAKEWPARAM(ID_MEDIA_ASSET_FIRST + assetIndex, 0));
        }
    }

    CWnd::OnLButtonDown(flags, point);
}

void EditorPane::drawMediaLibrary(CDC &deviceContext, const CRect &clientRect) const
{
    drawPaneTitle(deviceContext, _T("Media Library (MFC baseline)"));

    CRect categoryRect(10, kHeaderHeight + 8, clientRect.Width() - 10, kHeaderHeight + 38);
    deviceContext.FillSolidRect(categoryRect, RGB(49, 53, 62));
    drawText(deviceContext, _T("My Media    Stock Media    Backgrounds"), categoryRect,
             kText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const auto &assets = demoAssets();
    for (int index = 0; index < static_cast<int>(assets.size()); ++index) {
        const int column = index % 2;
        const int row = index / 2;
        const int x = 12 + column * (kAssetWidth + kAssetSpacing);
        const int y = kHeaderHeight + 54 + row * (kAssetHeight + kAssetSpacing);
        const CRect assetRect(x, y, x + kAssetWidth, y + kAssetHeight);

        if (index == selectedAssetIndex_)
            deviceContext.Draw3dRect(assetRect, kAccent, kAccent);

        const CRect thumbnailRect(assetRect.left + 4, assetRect.top + 4,
                                  assetRect.right - 4, assetRect.top + 70);
        deviceContext.FillSolidRect(thumbnailRect, assets[index].thumbnailColor);

        CRect typeRect(thumbnailRect.left + 5, thumbnailRect.top + 5,
                       thumbnailRect.left + 55, thumbnailRect.top + 24);
        deviceContext.FillSolidRect(typeRect, RGB(25, 27, 32));
        drawText(deviceContext, assets[index].kind, typeRect, kText,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        CRect nameRect(assetRect.left + 4, assetRect.top + 75,
                       assetRect.right - 4, assetRect.top + 95);
        drawText(deviceContext, assets[index].name, nameRect, kText,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        CRect durationRect(assetRect.left + 4, assetRect.top + 95,
                           assetRect.right - 4, assetRect.bottom - 4);
        drawText(deviceContext, assets[index].duration, durationRect, kSecondaryText,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void EditorPane::drawPreview(CDC &deviceContext, const CRect &clientRect) const
{
    drawPaneTitle(deviceContext, _T("Preview"));

    const auto &asset = demoAssets()[selectedAssetIndex_];
    const CRect availableRect(20, kHeaderHeight + 18,
                              clientRect.right - 20, clientRect.bottom - 66);
    const int availableWidth = availableRect.Width();
    const int availableHeight = availableRect.Height();
    const int videoHeight = std::min(availableHeight, availableWidth * 9 / 16);
    const int videoWidth = videoHeight * 16 / 9;
    const int videoLeft = availableRect.left + (availableWidth - videoWidth) / 2;
    const int videoTop = availableRect.top + (availableHeight - videoHeight) / 2;
    const CRect videoRect(videoLeft, videoTop, videoLeft + videoWidth, videoTop + videoHeight);

    deviceContext.FillSolidRect(availableRect, kCanvasBackground);
    deviceContext.FillSolidRect(videoRect, asset.thumbnailColor);
    deviceContext.Draw3dRect(videoRect, RGB(220, 220, 220), RGB(220, 220, 220));

    CRect titleRect(videoRect.left + 12, videoRect.bottom - 38,
                    videoRect.right - 12, videoRect.bottom - 12);
    drawText(deviceContext, asset.name, titleRect, RGB(255, 255, 255),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    CRect controlsRect(20, clientRect.bottom - 52, clientRect.right - 20, clientRect.bottom - 14);
    deviceContext.FillSolidRect(controlsRect, RGB(27, 29, 34));
    drawText(deviceContext, _T("|<    <    Play / Pause    >    >|"), controlsRect,
             kSecondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void EditorPane::drawProperties(CDC &deviceContext, const CRect &clientRect) const
{
    drawPaneTitle(deviceContext, _T("Properties"));

    const auto &asset = demoAssets()[selectedAssetIndex_];
    const int left = 16;
    int top = kHeaderHeight + 18;

    const CString rows[] = {
        _T("Selected clip"),
        CString(_T("Name: ")) + asset.name,
        CString(_T("Type: ")) + asset.kind,
        CString(_T("Duration: ")) + asset.duration,
        _T("Opacity: 100%"),
        _T("Position: Center"),
        _T("Scale: 100%")
    };

    for (int index = 0; index < _countof(rows); ++index) {
        const CRect rowRect(left, top, clientRect.right - left, top + 31);
        if (index == 0) {
            drawText(deviceContext, rows[index], rowRect, kText);
        } else {
            deviceContext.FillSolidRect(rowRect, RGB(45, 48, 56));
            drawText(deviceContext, rows[index], rowRect, kSecondaryText);
        }
        top += 39;
    }
}

void EditorPane::drawTimeline(CDC &deviceContext, const CRect &clientRect) const
{
    drawPaneTitle(deviceContext, _T("Timeline"));

    const int rulerTop = kHeaderHeight;
    const int trackTop = rulerTop + 30;
    const int trackHeight = 66;
    deviceContext.FillSolidRect(0, rulerTop, clientRect.Width(), 30, RGB(28, 30, 35));
    deviceContext.FillSolidRect(0, trackTop, clientRect.Width(), trackHeight, RGB(43, 46, 54));
    deviceContext.FillSolidRect(0, trackTop + trackHeight + 8,
                                clientRect.Width(), trackHeight, RGB(38, 41, 48));

    for (int x = 110; x < clientRect.Width(); x += 80) {
        deviceContext.FillSolidRect(x, rulerTop + 18, 1, 12, kSecondaryText);
        CString timeLabel;
        timeLabel.Format(_T("%d"), (x - 110) / 8);
        drawText(deviceContext, timeLabel, CRect(x + 3, rulerTop + 2, x + 53, rulerTop + 18),
                 kSecondaryText);
    }

    drawText(deviceContext, _T("V1"), CRect(12, trackTop, 60, trackTop + trackHeight),
             kSecondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    drawText(deviceContext, _T("A1"), CRect(12, trackTop + trackHeight + 8,
                                              60, trackTop + trackHeight * 2 + 8),
             kSecondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const auto &asset = demoAssets()[selectedAssetIndex_];
    const CRect clipRect(76, trackTop + 5, 410, trackTop + trackHeight - 5);
    deviceContext.FillSolidRect(clipRect, asset.thumbnailColor);
    deviceContext.Draw3dRect(clipRect, RGB(180, 220, 255), RGB(180, 220, 255));
    drawText(deviceContext, asset.name, CRect(clipRect.left + 10, clipRect.top,
                                               clipRect.right - 10, clipRect.bottom),
             RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    deviceContext.FillSolidRect(76, trackTop + trackHeight + 16, 334, trackHeight - 16,
                                RGB(38, 114, 176));
    deviceContext.FillSolidRect(76, rulerTop, 2, clientRect.bottom - rulerTop, RGB(240, 74, 74));
}

void EditorPane::drawPaneTitle(CDC &deviceContext, const CString &title) const
{
    CRect clientRect;
    GetClientRect(&clientRect);
    const CRect titleRect(0, 0, clientRect.right, kHeaderHeight);
    deviceContext.FillSolidRect(titleRect, RGB(47, 50, 58));
    drawText(deviceContext, title, CRect(12, 0, clientRect.right - 12, kHeaderHeight), kText);
}

void EditorPane::drawText(CDC &deviceContext, const CString &text, const CRect &bounds,
                          COLORREF color, UINT format) const
{
    const int previousBackgroundMode = deviceContext.SetBkMode(TRANSPARENT);
    const COLORREF previousColor = deviceContext.SetTextColor(color);
    deviceContext.DrawText(text, const_cast<CRect *>(&bounds), format);
    deviceContext.SetTextColor(previousColor);
    deviceContext.SetBkMode(previousBackgroundMode);
}

int EditorPane::mediaAssetAt(CPoint point) const
{
    if (point.y < kHeaderHeight + 54)
        return -1;

    const int column = (point.x - 12) / (kAssetWidth + kAssetSpacing);
    const int row = (point.y - (kHeaderHeight + 54)) / (kAssetHeight + kAssetSpacing);
    if (column < 0 || column > 1 || row < 0)
        return -1;

    const int assetIndex = row * 2 + column;
    if (assetIndex >= static_cast<int>(demoAssets().size()))
        return -1;

    const int x = 12 + column * (kAssetWidth + kAssetSpacing);
    const int y = kHeaderHeight + 54 + row * (kAssetHeight + kAssetSpacing);
    const CRect assetRect(x, y, x + kAssetWidth, y + kAssetHeight);
    return assetRect.PtInRect(point) ? assetIndex : -1;
}
