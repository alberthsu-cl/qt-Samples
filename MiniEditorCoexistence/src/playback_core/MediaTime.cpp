#include "MediaTime.h"

#include <cassert>
#include <numeric>
#include <stdexcept>

namespace mini_editor::playback_core {
namespace {

constexpr std::int64_t kMicrosecondsPerSecond = 1'000'000;

void requirePositive(int value, const char *name)
{
    if (value <= 0)
        throw std::invalid_argument(name);
}

void requireNonNegative(std::int64_t value)
{
    assert(value >= 0);
}

std::int64_t ceilDivide(std::int64_t numerator, std::int64_t denominator)
{
    assert(numerator >= 0 && denominator > 0);
    return numerator / denominator + (numerator % denominator != 0 ? 1 : 0);
}

std::int64_t frameStartMicroseconds(TimelineFrame frame, FrameRate rate)
{
    // Ceiling gives the first representable microsecond at or after the exact
    // rational frame boundary. This preserves frameAtSequenceTime(start) ==
    // frame even when the boundary cannot be represented exactly in us.
    const std::int64_t numerator = frame.frameNumber()
        * kMicrosecondsPerSecond * rate.denominator();
    return ceilDivide(numerator, rate.numerator());
}

} // namespace

FrameRate::FrameRate(std::int32_t numerator, std::int32_t denominator)
    : numerator_(numerator)
    , denominator_(denominator)
{
    requirePositive(numerator, "FrameRate numerator must be positive.");
    requirePositive(denominator, "FrameRate denominator must be positive.");

    const std::int32_t divisor = std::gcd(numerator_, denominator_);
    numerator_ /= divisor;
    denominator_ /= divisor;
}

std::int32_t FrameRate::numerator() const { return numerator_; }
std::int32_t FrameRate::denominator() const { return denominator_; }
bool operator==(FrameRate left, FrameRate right)
{
    return left.numerator() == right.numerator()
        && left.denominator() == right.denominator();
}
bool operator!=(FrameRate left, FrameRate right) { return !(left == right); }

TimelineFrame::TimelineFrame(std::int64_t value) : value_(value)
{
    requireNonNegative(value);
}
TimelineFrame TimelineFrame::zero() { return TimelineFrame(0); }
TimelineFrame TimelineFrame::fromFrameNumber(std::int64_t value)
{
    return TimelineFrame(value);
}
std::int64_t TimelineFrame::frameNumber() const { return value_; }

FrameCount::FrameCount(std::int64_t value) : value_(value) {}
FrameCount FrameCount::zero() { return FrameCount(0); }
FrameCount FrameCount::fromFrames(std::int64_t value) { return FrameCount(value); }
std::int64_t FrameCount::frames() const { return value_; }

FrameCount operator-(TimelineFrame left, TimelineFrame right)
{
    return FrameCount::fromFrames(left.frameNumber() - right.frameNumber());
}
TimelineFrame operator+(TimelineFrame frame, FrameCount offset)
{
    return TimelineFrame::fromFrameNumber(frame.frameNumber() + offset.frames());
}
TimelineFrame operator-(TimelineFrame frame, FrameCount offset)
{
    return TimelineFrame::fromFrameNumber(frame.frameNumber() - offset.frames());
}
FrameCount operator+(FrameCount left, FrameCount right)
{
    return FrameCount::fromFrames(left.frames() + right.frames());
}
FrameCount operator-(FrameCount left, FrameCount right)
{
    return FrameCount::fromFrames(left.frames() - right.frames());
}
FrameCount operator-(FrameCount value) { return FrameCount::fromFrames(-value.frames()); }

bool operator==(TimelineFrame left, TimelineFrame right)
{
    return left.frameNumber() == right.frameNumber();
}
bool operator!=(TimelineFrame left, TimelineFrame right) { return !(left == right); }
bool operator<(TimelineFrame left, TimelineFrame right)
{
    return left.frameNumber() < right.frameNumber();
}
bool operator<=(TimelineFrame left, TimelineFrame right) { return !(right < left); }
bool operator>(TimelineFrame left, TimelineFrame right) { return right < left; }
bool operator>=(TimelineFrame left, TimelineFrame right) { return !(left < right); }

bool operator==(FrameCount left, FrameCount right) { return left.frames() == right.frames(); }
bool operator!=(FrameCount left, FrameCount right) { return !(left == right); }
bool operator<(FrameCount left, FrameCount right) { return left.frames() < right.frames(); }
bool operator<=(FrameCount left, FrameCount right) { return !(right < left); }
bool operator>(FrameCount left, FrameCount right) { return right < left; }
bool operator>=(FrameCount left, FrameCount right) { return !(left < right); }

SourceTimestamp::SourceTimestamp(std::int64_t value) : value_(value) {}
SourceTimestamp SourceTimestamp::fromMicroseconds(std::int64_t value)
{
    return SourceTimestamp(value);
}
std::int64_t SourceTimestamp::microsecondsForAdapter() const { return value_; }

SequenceTime::SequenceTime(std::int64_t value) : value_(value)
{
    requireNonNegative(value);
}
SequenceTime SequenceTime::zero() { return SequenceTime(0); }
SequenceTime SequenceTime::fromMicroseconds(std::int64_t value)
{
    return SequenceTime(value);
}
std::int64_t SequenceTime::microsecondsForAdapter() const { return value_; }

SequenceDuration::SequenceDuration(std::int64_t value) : value_(value) {}
SequenceDuration SequenceDuration::zero() { return SequenceDuration(0); }
SequenceDuration SequenceDuration::fromMicroseconds(std::int64_t value)
{
    return SequenceDuration(value);
}
std::int64_t SequenceDuration::microsecondsForAdapter() const { return value_; }

MasterClockTime::MasterClockTime(std::int64_t value) : value_(value) {}
MasterClockTime MasterClockTime::fromMicroseconds(std::int64_t value)
{
    return MasterClockTime(value);
}
std::int64_t MasterClockTime::microsecondsForClockAdapter() const { return value_; }

ClockDuration::ClockDuration(std::int64_t value) : value_(value) {}
ClockDuration ClockDuration::zero() { return ClockDuration(0); }
ClockDuration ClockDuration::fromMicroseconds(std::int64_t value)
{
    return ClockDuration(value);
}
std::int64_t ClockDuration::microsecondsForClockAdapter() const { return value_; }

SequenceDuration operator-(SequenceTime left, SequenceTime right)
{
    return SequenceDuration::fromMicroseconds(
        left.microsecondsForAdapter() - right.microsecondsForAdapter());
}
SequenceTime operator+(SequenceTime time, SequenceDuration duration)
{
    return SequenceTime::fromMicroseconds(
        time.microsecondsForAdapter() + duration.microsecondsForAdapter());
}
SequenceTime operator-(SequenceTime time, SequenceDuration duration)
{
    return SequenceTime::fromMicroseconds(
        time.microsecondsForAdapter() - duration.microsecondsForAdapter());
}
SequenceDuration operator+(SequenceDuration left, SequenceDuration right)
{
    return SequenceDuration::fromMicroseconds(
        left.microsecondsForAdapter() + right.microsecondsForAdapter());
}
SequenceDuration operator-(SequenceDuration left, SequenceDuration right)
{
    return SequenceDuration::fromMicroseconds(
        left.microsecondsForAdapter() - right.microsecondsForAdapter());
}
SequenceDuration operator-(SequenceDuration value)
{
    return SequenceDuration::fromMicroseconds(-value.microsecondsForAdapter());
}

ClockDuration operator-(MasterClockTime left, MasterClockTime right)
{
    return ClockDuration::fromMicroseconds(
        left.microsecondsForClockAdapter() - right.microsecondsForClockAdapter());
}
MasterClockTime operator+(MasterClockTime time, ClockDuration duration)
{
    return MasterClockTime::fromMicroseconds(
        time.microsecondsForClockAdapter() + duration.microsecondsForClockAdapter());
}
MasterClockTime operator-(MasterClockTime time, ClockDuration duration)
{
    return MasterClockTime::fromMicroseconds(
        time.microsecondsForClockAdapter() - duration.microsecondsForClockAdapter());
}
ClockDuration operator+(ClockDuration left, ClockDuration right)
{
    return ClockDuration::fromMicroseconds(
        left.microsecondsForClockAdapter() + right.microsecondsForClockAdapter());
}
ClockDuration operator-(ClockDuration left, ClockDuration right)
{
    return ClockDuration::fromMicroseconds(
        left.microsecondsForClockAdapter() - right.microsecondsForClockAdapter());
}
ClockDuration operator-(ClockDuration value)
{
    return ClockDuration::fromMicroseconds(-value.microsecondsForClockAdapter());
}

bool operator==(SourceTimestamp left, SourceTimestamp right)
{
    return left.microsecondsForAdapter() == right.microsecondsForAdapter();
}
bool operator!=(SourceTimestamp left, SourceTimestamp right) { return !(left == right); }
bool operator<(SourceTimestamp left, SourceTimestamp right)
{
    return left.microsecondsForAdapter() < right.microsecondsForAdapter();
}
bool operator<=(SourceTimestamp left, SourceTimestamp right) { return !(right < left); }
bool operator>(SourceTimestamp left, SourceTimestamp right) { return right < left; }
bool operator>=(SourceTimestamp left, SourceTimestamp right) { return !(left < right); }

bool operator==(SequenceTime left, SequenceTime right)
{
    return left.microsecondsForAdapter() == right.microsecondsForAdapter();
}
bool operator!=(SequenceTime left, SequenceTime right) { return !(left == right); }
bool operator<(SequenceTime left, SequenceTime right)
{
    return left.microsecondsForAdapter() < right.microsecondsForAdapter();
}
bool operator<=(SequenceTime left, SequenceTime right) { return !(right < left); }
bool operator>(SequenceTime left, SequenceTime right) { return right < left; }
bool operator>=(SequenceTime left, SequenceTime right) { return !(left < right); }

bool operator==(SequenceDuration left, SequenceDuration right)
{
    return left.microsecondsForAdapter() == right.microsecondsForAdapter();
}
bool operator!=(SequenceDuration left, SequenceDuration right) { return !(left == right); }
bool operator<(SequenceDuration left, SequenceDuration right)
{
    return left.microsecondsForAdapter() < right.microsecondsForAdapter();
}
bool operator<=(SequenceDuration left, SequenceDuration right) { return !(right < left); }
bool operator>(SequenceDuration left, SequenceDuration right) { return right < left; }
bool operator>=(SequenceDuration left, SequenceDuration right) { return !(left < right); }

bool operator==(MasterClockTime left, MasterClockTime right)
{
    return left.microsecondsForClockAdapter()
        == right.microsecondsForClockAdapter();
}
bool operator!=(MasterClockTime left, MasterClockTime right) { return !(left == right); }
bool operator<(MasterClockTime left, MasterClockTime right)
{
    return left.microsecondsForClockAdapter()
        < right.microsecondsForClockAdapter();
}
bool operator<=(MasterClockTime left, MasterClockTime right) { return !(right < left); }
bool operator>(MasterClockTime left, MasterClockTime right) { return right < left; }
bool operator>=(MasterClockTime left, MasterClockTime right) { return !(left < right); }

bool operator==(ClockDuration left, ClockDuration right)
{
    return left.microsecondsForClockAdapter()
        == right.microsecondsForClockAdapter();
}
bool operator!=(ClockDuration left, ClockDuration right) { return !(left == right); }
bool operator<(ClockDuration left, ClockDuration right)
{
    return left.microsecondsForClockAdapter()
        < right.microsecondsForClockAdapter();
}
bool operator<=(ClockDuration left, ClockDuration right) { return !(right < left); }
bool operator>(ClockDuration left, ClockDuration right) { return right < left; }
bool operator>=(ClockDuration left, ClockDuration right) { return !(left < right); }

SequenceTime sequenceTimeAtFrameStart(TimelineFrame frame, FrameRate sequenceRate)
{
    return SequenceTime::fromMicroseconds(frameStartMicroseconds(frame, sequenceRate));
}

SequenceTime sequenceTimeAtNextFrameStart(TimelineFrame frame, FrameRate sequenceRate)
{
    return sequenceTimeAtFrameStart(
        frame + FrameCount::fromFrames(1), sequenceRate);
}

TimelineFrame frameAtSequenceTime(SequenceTime time, FrameRate sequenceRate)
{
    const std::int64_t numerator = time.microsecondsForAdapter()
        * sequenceRate.numerator();
    const std::int64_t denominator = kMicrosecondsPerSecond
        * sequenceRate.denominator();
    return TimelineFrame::fromFrameNumber(numerator / denominator);
}

std::optional<SourceTimestamp> sourceTimestampFor(
    TimelineFrame frame, const ClipTimeMapping &mapping)
{
    if (!mapping.sourceIn || frame < mapping.clipStartFrame)
        return std::nullopt;

    const FrameCount localFrames = frame - mapping.clipStartFrame;
    const SequenceTime localTime = sequenceTimeAtFrameStart(
        TimelineFrame::fromFrameNumber(localFrames.frames()), mapping.sequenceRate);
    return SourceTimestamp::fromMicroseconds(
        mapping.sourceIn->microsecondsForAdapter()
        + localTime.microsecondsForAdapter());
}

SequenceDuration sequenceElapsedFor(ClockDuration elapsed,
                                    int playbackRatePercent)
{
    requirePositive(playbackRatePercent, "Playback rate must be positive.");
    return SequenceDuration::fromMicroseconds(
        elapsed.microsecondsForClockAdapter() * playbackRatePercent / 100);
}

ClockDuration clockElapsedFor(SequenceDuration elapsed,
                              int playbackRatePercent)
{
    requirePositive(playbackRatePercent, "Playback rate must be positive.");
    return ClockDuration::fromMicroseconds(
        elapsed.microsecondsForAdapter() * 100 / playbackRatePercent);
}

} // namespace mini_editor::playback_core
