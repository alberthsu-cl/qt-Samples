#pragma once

#include <afxwin.h>

#include <array>

// This is the framework-neutral project data. Phase 1 will let a Qt model
// present these same assets without putting UI-specific state into the data.
struct MediaAsset {
    const wchar_t *name;
    const wchar_t *kind;
    const wchar_t *duration;
    COLORREF thumbnailColor;
};

inline const std::array<MediaAsset, 6> &demoAssets()
{
    static const std::array<MediaAsset, 6> assets = {{
        { L"Mountainbike.mp4", L"Video", L"00:00:10", RGB(174, 116, 45) },
        { L"Food.jpg",          L"Image", L"Still",    RGB(179, 77, 44) },
        { L"Forest.jpg",        L"Image", L"Still",    RGB(47, 112, 65) },
        { L"Mahoroba.mp3",      L"Audio", L"00:02:19", RGB(40, 120, 180) },
        { L"Skateboard.mp4",    L"Video", L"00:00:10", RGB(77, 145, 184) },
        { L"Sport.jpg",         L"Image", L"Still",    RGB(126, 94, 72) },
    }};

    return assets;
}
