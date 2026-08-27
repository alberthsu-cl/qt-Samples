#include "ClipFade.h"

#include <algorithm>

namespace {

int clampedRequest(int frames)
{
    return std::clamp(frames, 0, ClipFade::kMaximumFadeFrames);
}

} // namespace

ClipFadeRange ClipFade::normalize(const ClipSettings &settings,
                                  int clipDurationFrames)
{
    const int duration = std::max(0, clipDurationFrames);
    int fadeIn = clampedRequest(settings.fadeInFrames);
    int fadeOut = clampedRequest(settings.fadeOutFrames);

    // Overlapping ramps have no meaningful value, so give each one the share
    // of the clip that its request asked for and let them meet in the middle.
    if (fadeIn + fadeOut > duration) {
        const int total = fadeIn + fadeOut;
        fadeIn = duration * fadeIn / total;
        fadeOut = duration - fadeIn;
    }
    return { fadeIn, fadeOut };
}

int ClipFade::gainPercentAt(const ClipSettings &settings, int clipLocalFrame,
                            int clipDurationFrames)
{
    const int duration = std::max(0, clipDurationFrames);
    if (duration <= 0)
        return 100;

    const int frame = std::clamp(clipLocalFrame, 0, duration - 1);
    const ClipFadeRange range = normalize(settings, duration);

    int gain = 100;
    if (range.fadeInFrames > 0 && frame < range.fadeInFrames)
        gain = frame * 100 / range.fadeInFrames;

    if (range.fadeOutFrames > 0) {
        const int framesRemaining = duration - frame;
        if (framesRemaining <= range.fadeOutFrames)
            gain = std::min(gain, framesRemaining * 100 / range.fadeOutFrames);
    }
    return std::clamp(gain, 0, 100);
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
        settings.fadeInFrames = clampedRequest(settings.fadeInFrames);
        settings.fadeOutFrames = clampedRequest(settings.fadeOutFrames);
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
