#include "SteadyPlaybackClock.h"

#include <chrono>

namespace mini_editor::playback_core {

MasterClockTime SteadyPlaybackClock::now() const
{
    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    return MasterClockTime::fromMicroseconds(microseconds);
}

} // namespace mini_editor::playback_core
