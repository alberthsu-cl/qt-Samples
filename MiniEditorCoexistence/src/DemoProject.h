#pragma once

#include <afxwin.h>

#include <array>
#include <algorithm>

// MFC/Windows headers define min/max macros unless NOMINMAX is set by the
// build. Keep this framework-neutral data header safe for std::min/std::max
// callers such as EditorSession and core tests.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// This is the framework-neutral project data. Phase 1 will let a Qt model
// present these same assets without putting UI-specific state into the data.
struct MediaAsset {
    int id;
    const wchar_t *name;
    const wchar_t *kind;
    const wchar_t *duration;
    int timelineDurationFrames;
    COLORREF thumbnailColor;
};

inline const std::array<MediaAsset, 6> &demoAssets()
{
    static const std::array<MediaAsset, 6> assets = {{
        { 1, L"Mountainbike.mp4", L"Video", L"00:00:10", 300,  RGB(174, 116, 45) },
        { 2, L"Food.jpg",          L"Image", L"Still",    90,   RGB(179, 77, 44) },
        { 3, L"Forest.jpg",        L"Image", L"Still",    90,   RGB(47, 112, 65) },
        { 4, L"Mahoroba.mp3",      L"Audio", L"00:02:19", 4170, RGB(40, 120, 180) },
        { 5, L"Skateboard.mp4",    L"Video", L"00:00:10", 300,  RGB(77, 145, 184) },
        { 6, L"Sport.jpg",         L"Image", L"Still",    90,   RGB(126, 94, 72) },
    }};

    return assets;
}

inline const MediaAsset *findDemoAsset(int assetId)
{
    const auto &assets = demoAssets();
    const auto iterator = std::find_if(assets.begin(), assets.end(),
        [assetId](const MediaAsset &asset) { return asset.id == assetId; });
    return iterator == assets.end() ? nullptr : &*iterator;
}
