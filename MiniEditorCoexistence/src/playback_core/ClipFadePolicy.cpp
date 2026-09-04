#include "ClipFadePolicy.h"

#include <algorithm>

namespace mini_editor::playback_core {
namespace {

int clampedRequest(int frames)
{
    return std::clamp(frames, 0, kMaximumFadeFrames);
}

} // namespace

ClipFadeRange normalizeClipFade(int fadeInFrames, int fadeOutFrames,
                                int clipDurationFrames)
{
    const int duration = std::max(0, clipDurationFrames);
    int fadeIn = clampedRequest(fadeInFrames);
    int fadeOut = clampedRequest(fadeOutFrames);

    // Overlapping ramps have no meaningful value, so give each one the share
    // of the clip that its request asked for and let them meet in the middle.
    if (fadeIn + fadeOut > duration) {
        const int total = fadeIn + fadeOut;
        fadeIn = duration * fadeIn / total;
        fadeOut = duration - fadeIn;
    }
    return { fadeIn, fadeOut };
}

int clipFadeGainPercentAt(int fadeInFrames, int fadeOutFrames,
                          int clipLocalFrame, int clipDurationFrames)
{
    const int duration = std::max(0, clipDurationFrames);
    if (duration <= 0)
        return 100;

    const int frame = std::clamp(clipLocalFrame, 0, duration - 1);
    const ClipFadeRange range = normalizeClipFade(fadeInFrames, fadeOutFrames, duration);

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

} // namespace mini_editor::playback_core
