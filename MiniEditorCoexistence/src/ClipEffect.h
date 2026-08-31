#pragma once

// Framework-neutral DSP effect selection for one timeline placement. This is
// the same idea as ThreadedEffectPreview's EffectType, promoted from a
// transient UI choice into project data: the editor stores it per clip, so it
// takes part in Undo/Redo and is written to the project file.
//
// The pixel work itself is deliberately NOT here. This header must stay usable
// by the core tests, which link neither Qt nor MFC.
enum class ClipEffectKind {
    None,
    Grayscale,
    Invert,
    Blur
};

inline const wchar_t *clipEffectDisplayName(ClipEffectKind effect)
{
    switch (effect) {
    case ClipEffectKind::None:
        return L"None";
    case ClipEffectKind::Grayscale:
        return L"Grayscale";
    case ClipEffectKind::Invert:
        return L"Invert";
    case ClipEffectKind::Blur:
        return L"Blur";
    }

    return L"None";
}

// Intensity blends the processed frame back over the original, so every effect
// gets a usable strength control instead of being an all-or-nothing switch.
constexpr int kMinimumEffectIntensityPercent = 0;
constexpr int kMaximumEffectIntensityPercent = 100;
