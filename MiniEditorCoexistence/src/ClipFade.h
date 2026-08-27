#pragma once

#include "ProjectState.h"

// The fade lengths that actually apply to one placement. They may be shorter
// than the stored request when a trim made the clip shorter than the two
// ramps combined.
struct ClipFadeRange {
    int fadeInFrames = 0;
    int fadeOutFrames = 0;
};

// Framework-neutral fade policy. A fade is an edit decision on one placement,
// so it lives beside ClipSettings rather than in a renderer. Both the Qt
// preview (video opacity) and the timeline overlay (ramp drawing) ask this
// class the same question, which keeps the two surfaces consistent.
class ClipFade final
{
public:
    // Long enough for a slow ten-second dissolve at 30 FPS, short enough that
    // a stored value can never dominate the whole learning timeline.
    static constexpr int kMaximumFadeFrames = 300;

    // Clamps negative requests away and, when the two ramps would overlap,
    // shortens them proportionally so they exactly meet inside the clip.
    static ClipFadeRange normalize(const ClipSettings &settings,
                                   int clipDurationFrames);

    // Returns the ramp value at one clip-local frame as a percentage. Video
    // multiplies it into opacity; audio would multiply it into level.
    static int gainPercentAt(const ClipSettings &settings,
                             int clipLocalFrame, int clipDurationFrames);

    static int effectiveOpacityPercent(const ClipSettings &settings,
                                       int clipLocalFrame, int clipDurationFrames);

    // Stores fade lengths that are already valid for this clip duration, so
    // Undo/Redo and the project file never carry an impossible request.
    static ClipSettings clampSettings(ClipSettings settings,
                                      int clipDurationFrames);

    static bool hasFade(const ClipSettings &settings);
};
