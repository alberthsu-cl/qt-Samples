#pragma once

// This header deliberately knows nothing about MFC or Qt. In Phase 2, the
// Qt dialog will use exactly this same data model.
enum class EffectType {
    None,
    Grayscale,
    Invert,
    Blur
};

struct EffectSettings {
    EffectType selectedEffect = EffectType::None;
};

inline const wchar_t *effectTypeDisplayName(EffectType effect)
{
    switch (effect) {
    case EffectType::None:
        return L"No effect";
    case EffectType::Grayscale:
        return L"Grayscale";
    case EffectType::Invert:
        return L"Invert";
    case EffectType::Blur:
        return L"Blur";
    }

    return L"Unknown effect";
}
