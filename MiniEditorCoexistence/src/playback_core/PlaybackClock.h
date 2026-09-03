#pragma once

#include "MediaTime.h"

namespace mini_editor::playback_core {

// ADR-004's injected clock port. Production supplies a steady_clock (or,
// later, audio-device) adapter; deterministic tests supply a controllable
// fake. The engine never calls std::chrono::steady_clock directly.
class IPlaybackClock {
public:
    virtual ~IPlaybackClock() = default;

    virtual MasterClockTime now() const = 0;
};

// One immutable transport-epoch anchor (ADR-004). The scheduler replaces the
// anchor atomically at each re-anchoring boundary; this milestone only
// resolves a position from an existing anchor. Deciding when to create a new
// one is PlaybackSession's responsibility (Milestone 3's session issue).
struct PlaybackAnchor final {
    MasterClockTime masterClock;
    SequenceTime sequenceTime;
    int playbackRatePercent = 100;
};

// ADR-004's anchor equation:
//   elapsedClock = clock.now() - anchor.masterClock
//   elapsedSequence = sequenceElapsedFor(elapsedClock, anchor.playbackRatePercent)
//   sequenceTime = anchor.sequenceTime + elapsedSequence
// Built only from the existing ADR-001 rate-conversion helper; it does not
// accumulate rounded frame durations.
SequenceTime resolveSequenceTime(const PlaybackAnchor &anchor,
                                 const IPlaybackClock &clock);

// Convenience composition of resolveSequenceTime() with frameAtSequenceTime()
// for callers that want the timeline frame directly.
TimelineFrame resolveTimelineFrame(const PlaybackAnchor &anchor,
                                   const IPlaybackClock &clock,
                                   FrameRate sequenceRate);

} // namespace mini_editor::playback_core
