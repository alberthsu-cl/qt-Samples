#pragma once

#include "PlaybackClock.h"

namespace mini_editor::playback_core {

// ADR-004's milestone-1 production clock: "Milestone 1 uses
// std::chrono::steady_clock for both cases because the existing Qt media
// bridge does not yet expose a sample-accurate device clock." This is pure
// C++ standard library -- framework-neutral, no Qt/MFC dependency, so it
// lives here unconditionally rather than behind MINI_EDITOR_USE_QT.
class SteadyPlaybackClock final : public IPlaybackClock {
public:
    MasterClockTime now() const override;
};

} // namespace mini_editor::playback_core
