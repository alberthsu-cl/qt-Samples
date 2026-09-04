#pragma once

#include "PlaybackCommand.h"

#include <cstdint>
#include <optional>

namespace mini_editor::playback_core {

// Everything a UI needs to paint a timeline transport, derived from the one
// authoritative PlaybackStatus.
//
// ADR-002's migration strategy allows the legacy EditorSession::PlaybackState
// to survive, but only as a painting cache populated from a status
// publication -- never read back as authority. This type is the shape of that
// publication, and it is deliberately output-only: there is no field here a
// UI could set to mean "and now play", so nothing can quietly turn the cache
// back into a second transport authority.
struct TimelineTransportView final {
    bool isPlaying = false;
    bool isPaused = false;
    std::int64_t timelineFrame = 0;
    std::int64_t durationFrames = 0;
    int framesPerSecond = 30;
    int playbackRatePercent = 100;
};

bool operator==(const TimelineTransportView &left, const TimelineTransportView &right);
bool operator!=(const TimelineTransportView &left, const TimelineTransportView &right);

// Empty for a status that is not sequence preview: a source-asset session has
// no timeline transport to paint, and inventing one would put a second
// meaning on the same cache.
//
// Only Playing reads as playing and only Paused as paused. Seeking and
// Prerolling are neither: they are transient phases that resolve inside a
// single applyCommand() in this milestone, so no UI ever observes one, and
// guessing which of the two visible states they resemble would be the kind of
// invented difference the M5-07 comparison exists to catch.
std::optional<TimelineTransportView> timelineTransportViewFor(const PlaybackStatus &status);

} // namespace mini_editor::playback_core
