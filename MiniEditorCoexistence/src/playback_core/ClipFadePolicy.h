#pragma once

namespace mini_editor::playback_core {

// The fade lengths that actually apply to one placement. They may be shorter
// than the stored request when a trim made the clip shorter than the two
// ramps combined.
struct ClipFadeRange final {
    int fadeInFrames = 0;
    int fadeOutFrames = 0;
};

// Long enough for a slow ten-second dissolve at 30 FPS, short enough that a
// stored value can never dominate the whole learning timeline.
constexpr int kMaximumFadeFrames = 300;

// The one fade policy in the codebase.
//
// M5-01 deliberately left fade gain out of the snapshot resolver rather than
// reimplement the application target's ClipFade here, because two copies of
// this arithmetic can disagree -- and a disagreement between the legacy path
// and the routed one is exactly what M5-07 is built to catch. M5-06 needs the
// gain for A1 audio levels, so the policy moved here instead and ClipFade now
// delegates to it. Both paths ask the same function.

// Clamps negative requests away and, when the two ramps would overlap,
// shortens them proportionally so they exactly meet inside the clip.
ClipFadeRange normalizeClipFade(int fadeInFrames, int fadeOutFrames,
                                int clipDurationFrames);

// The ramp value at one clip-local frame, as a percentage. Video multiplies
// it into opacity; audio multiplies it into level.
int clipFadeGainPercentAt(int fadeInFrames, int fadeOutFrames,
                          int clipLocalFrame, int clipDurationFrames);

} // namespace mini_editor::playback_core
