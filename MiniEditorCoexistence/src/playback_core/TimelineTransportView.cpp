#include "TimelineTransportView.h"

namespace mini_editor::playback_core {
namespace {

// PlaybackState carries a whole-number frame rate; FrameRate is rational.
// Rounding to nearest is what makes 30000/1001 paint as 30 rather than 29.
int wholeFramesPerSecond(FrameRate rate)
{
    return static_cast<int>((rate.numerator() + rate.denominator() / 2)
                            / rate.denominator());
}

} // namespace

bool operator==(const TimelineTransportView &left, const TimelineTransportView &right)
{
    return left.isPlaying == right.isPlaying
        && left.isPaused == right.isPaused
        && left.timelineFrame == right.timelineFrame
        && left.durationFrames == right.durationFrames
        && left.framesPerSecond == right.framesPerSecond
        && left.playbackRatePercent == right.playbackRatePercent;
}

bool operator!=(const TimelineTransportView &left, const TimelineTransportView &right)
{
    return !(left == right);
}

std::optional<TimelineTransportView> timelineTransportViewFor(const PlaybackStatus &status)
{
    const auto *sequence = std::get_if<SequencePreviewStatus>(&status.context);
    if (sequence == nullptr)
        return std::nullopt;

    return TimelineTransportView {
        status.phase == PlaybackPhase::Playing,
        status.phase == PlaybackPhase::Paused,
        sequence->timelineFrame.frameNumber(),
        sequence->sequenceDuration.frames(),
        wholeFramesPerSecond(sequence->frameRate),
        status.ratePercent
    };
}

} // namespace mini_editor::playback_core
