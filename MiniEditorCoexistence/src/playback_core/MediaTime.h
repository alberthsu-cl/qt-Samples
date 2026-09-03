#pragma once

#include <cstdint>
#include <optional>

namespace mini_editor::playback_core {

// A positive rational frame rate. The constructor normalizes its fraction, so
// 30/1 and 60/2 compare as equal rates.
class FrameRate final {
public:
    FrameRate(std::int32_t numerator, std::int32_t denominator);

    std::int32_t numerator() const;
    std::int32_t denominator() const;

private:
    std::int32_t numerator_;
    std::int32_t denominator_;
};

bool operator==(FrameRate left, FrameRate right);
bool operator!=(FrameRate left, FrameRate right);

// Position and duration on a sequence's frame grid.
class TimelineFrame final {
public:
    static TimelineFrame zero();
    static TimelineFrame fromFrameNumber(std::int64_t value);

    std::int64_t frameNumber() const;

private:
    explicit TimelineFrame(std::int64_t value);

    std::int64_t value_;
};

class FrameCount final {
public:
    static FrameCount zero();
    static FrameCount fromFrames(std::int64_t value);

    std::int64_t frames() const;

private:
    explicit FrameCount(std::int64_t value);

    std::int64_t value_;
};

FrameCount operator-(TimelineFrame left, TimelineFrame right);
TimelineFrame operator+(TimelineFrame frame, FrameCount offset);
TimelineFrame operator-(TimelineFrame frame, FrameCount offset);
FrameCount operator+(FrameCount left, FrameCount right);
FrameCount operator-(FrameCount left, FrameCount right);
FrameCount operator-(FrameCount value);

bool operator==(TimelineFrame left, TimelineFrame right);
bool operator!=(TimelineFrame left, TimelineFrame right);
bool operator<(TimelineFrame left, TimelineFrame right);
bool operator<=(TimelineFrame left, TimelineFrame right);
bool operator>(TimelineFrame left, TimelineFrame right);
bool operator>=(TimelineFrame left, TimelineFrame right);

bool operator==(FrameCount left, FrameCount right);
bool operator!=(FrameCount left, FrameCount right);
bool operator<(FrameCount left, FrameCount right);
bool operator<=(FrameCount left, FrameCount right);
bool operator>(FrameCount left, FrameCount right);
bool operator>=(FrameCount left, FrameCount right);

// Three time domains, each stored as microseconds but intentionally kept as a
// distinct C++ type. The named factories/accessors are adapter boundaries.
class SourceTimestamp final {
public:
    static SourceTimestamp fromMicroseconds(std::int64_t value);

    std::int64_t microsecondsForAdapter() const;

private:
    explicit SourceTimestamp(std::int64_t value);

    std::int64_t value_;
};

class SequenceTime final {
public:
    static SequenceTime zero();
    static SequenceTime fromMicroseconds(std::int64_t value);

    std::int64_t microsecondsForAdapter() const;

private:
    explicit SequenceTime(std::int64_t value);

    std::int64_t value_;
};

class SequenceDuration final {
public:
    static SequenceDuration zero();
    static SequenceDuration fromMicroseconds(std::int64_t value);

    std::int64_t microsecondsForAdapter() const;

private:
    explicit SequenceDuration(std::int64_t value);

    std::int64_t value_;
};

class MasterClockTime final {
public:
    static MasterClockTime fromMicroseconds(std::int64_t value);

    std::int64_t microsecondsForClockAdapter() const;

private:
    explicit MasterClockTime(std::int64_t value);

    std::int64_t value_;
};

class ClockDuration final {
public:
    static ClockDuration zero();
    static ClockDuration fromMicroseconds(std::int64_t value);

    std::int64_t microsecondsForClockAdapter() const;

private:
    explicit ClockDuration(std::int64_t value);

    std::int64_t value_;
};

SequenceDuration operator-(SequenceTime left, SequenceTime right);
SequenceTime operator+(SequenceTime time, SequenceDuration duration);
SequenceTime operator-(SequenceTime time, SequenceDuration duration);
SequenceDuration operator+(SequenceDuration left, SequenceDuration right);
SequenceDuration operator-(SequenceDuration left, SequenceDuration right);
SequenceDuration operator-(SequenceDuration value);

ClockDuration operator-(MasterClockTime left, MasterClockTime right);
MasterClockTime operator+(MasterClockTime time, ClockDuration duration);
MasterClockTime operator-(MasterClockTime time, ClockDuration duration);
ClockDuration operator+(ClockDuration left, ClockDuration right);
ClockDuration operator-(ClockDuration left, ClockDuration right);
ClockDuration operator-(ClockDuration value);

bool operator==(SourceTimestamp left, SourceTimestamp right);
bool operator!=(SourceTimestamp left, SourceTimestamp right);
bool operator<(SourceTimestamp left, SourceTimestamp right);
bool operator<=(SourceTimestamp left, SourceTimestamp right);
bool operator>(SourceTimestamp left, SourceTimestamp right);
bool operator>=(SourceTimestamp left, SourceTimestamp right);

bool operator==(SequenceTime left, SequenceTime right);
bool operator!=(SequenceTime left, SequenceTime right);
bool operator<(SequenceTime left, SequenceTime right);
bool operator<=(SequenceTime left, SequenceTime right);
bool operator>(SequenceTime left, SequenceTime right);
bool operator>=(SequenceTime left, SequenceTime right);

bool operator==(SequenceDuration left, SequenceDuration right);
bool operator!=(SequenceDuration left, SequenceDuration right);
bool operator<(SequenceDuration left, SequenceDuration right);
bool operator<=(SequenceDuration left, SequenceDuration right);
bool operator>(SequenceDuration left, SequenceDuration right);
bool operator>=(SequenceDuration left, SequenceDuration right);

bool operator==(MasterClockTime left, MasterClockTime right);
bool operator!=(MasterClockTime left, MasterClockTime right);
bool operator<(MasterClockTime left, MasterClockTime right);
bool operator<=(MasterClockTime left, MasterClockTime right);
bool operator>(MasterClockTime left, MasterClockTime right);
bool operator>=(MasterClockTime left, MasterClockTime right);

bool operator==(ClockDuration left, ClockDuration right);
bool operator!=(ClockDuration left, ClockDuration right);
bool operator<(ClockDuration left, ClockDuration right);
bool operator<=(ClockDuration left, ClockDuration right);
bool operator>(ClockDuration left, ClockDuration right);
bool operator>=(ClockDuration left, ClockDuration right);

SequenceTime sequenceTimeAtFrameStart(TimelineFrame frame,
                                      FrameRate sequenceRate);
SequenceTime sequenceTimeAtNextFrameStart(TimelineFrame frame,
                                          FrameRate sequenceRate);
TimelineFrame frameAtSequenceTime(SequenceTime time, FrameRate sequenceRate);

struct ClipTimeMapping final {
    TimelineFrame clipStartFrame;
    std::optional<SourceTimestamp> sourceIn;
    FrameRate sequenceRate;
};

std::optional<SourceTimestamp> sourceTimestampFor(
    TimelineFrame frame, const ClipTimeMapping &mapping);

SequenceDuration sequenceElapsedFor(ClockDuration elapsed,
                                    int playbackRatePercent);
ClockDuration clockElapsedFor(SequenceDuration elapsed,
                              int playbackRatePercent);

} // namespace mini_editor::playback_core
