#include "ClipFade.h"

#include <algorithm>

ClipFadeRange ClipFade::normalize(const ClipSettings &settings,
                                  int clipDurationFrames)
{
    return mini_editor::playback_core::normalizeClipFade(
        settings.fadeInFrames, settings.fadeOutFrames, clipDurationFrames);
}

int ClipFade::gainPercentAt(const ClipSettings &settings, int clipLocalFrame,
                            int clipDurationFrames)
{
    return mini_editor::playback_core::clipFadeGainPercentAt(
        settings.fadeInFrames, settings.fadeOutFrames, clipLocalFrame,
        clipDurationFrames);
}

int ClipFade::effectiveOpacityPercent(const ClipSettings &settings,
                                      int clipLocalFrame, int clipDurationFrames)
{
    return settings.opacityPercent
        * gainPercentAt(settings, clipLocalFrame, clipDurationFrames) / 100;
}

ClipSettings ClipFade::clampSettings(ClipSettings settings, int clipDurationFrames)
{
    if (clipDurationFrames <= 0) {
        // Source-library settings have no placement duration yet. Keep the
        // request inside the supported range and validate it at drop time.
        settings.fadeInFrames = std::clamp(settings.fadeInFrames, 0, kMaximumFadeFrames);
        settings.fadeOutFrames = std::clamp(settings.fadeOutFrames, 0, kMaximumFadeFrames);
        return settings;
    }

    const ClipFadeRange range = normalize(settings, clipDurationFrames);
    settings.fadeInFrames = range.fadeInFrames;
    settings.fadeOutFrames = range.fadeOutFrames;
    return settings;
}

bool ClipFade::hasFade(const ClipSettings &settings)
{
    return settings.fadeInFrames > 0 || settings.fadeOutFrames > 0;
}
