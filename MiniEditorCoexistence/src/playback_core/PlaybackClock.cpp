#include "PlaybackClock.h"

namespace mini_editor::playback_core {

SequenceTime resolveSequenceTime(const PlaybackAnchor &anchor,
                                 const IPlaybackClock &clock)
{
    const ClockDuration elapsedClock = clock.now() - anchor.masterClock;
    const SequenceDuration elapsedSequence =
        sequenceElapsedFor(elapsedClock, anchor.playbackRatePercent);
    return anchor.sequenceTime + elapsedSequence;
}

TimelineFrame resolveTimelineFrame(const PlaybackAnchor &anchor,
                                   const IPlaybackClock &clock,
                                   FrameRate sequenceRate)
{
    return frameAtSequenceTime(resolveSequenceTime(anchor, clock), sequenceRate);
}

} // namespace mini_editor::playback_core
