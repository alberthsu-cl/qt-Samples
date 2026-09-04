#pragma once

#include "ProjectState.h"
#include "playback_core/ClipFadePolicy.h"

using ClipFadeRange = mini_editor::playback_core::ClipFadeRange;

// The ClipSettings-shaped face of the fade policy. A fade is an edit decision
// on one placement, so it lives beside ClipSettings rather than in a
// renderer. Both the Qt preview (video opacity) and the timeline overlay
// (ramp drawing) ask this class the same question, which keeps the two
// surfaces consistent.
//
// M5-06 moved the arithmetic itself into playback_core's ClipFadePolicy, so
// the routed path's A1 audio levels and this path's opacity come from one
// implementation rather than two that can drift.
class ClipFade final
{
public:
    static constexpr int kMaximumFadeFrames =
        mini_editor::playback_core::kMaximumFadeFrames;

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
