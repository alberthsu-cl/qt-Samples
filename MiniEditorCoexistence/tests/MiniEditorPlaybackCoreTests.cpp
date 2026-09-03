#include "PlaybackCoreBoundary.h"
#include "MediaTime.h"

#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {

template <typename Left, typename Right, typename = void>
struct HasAddition : std::false_type {};

template <typename Left, typename Right>
struct HasAddition<Left, Right,
                   std::void_t<decltype(std::declval<Left>()
                                        + std::declval<Right>())>>
    : std::true_type {};

template <typename Left, typename Right, typename = void>
struct HasSubtraction : std::false_type {};

template <typename Left, typename Right>
struct HasSubtraction<Left, Right,
                      std::void_t<decltype(std::declval<Left>()
                                           - std::declval<Right>())>>
    : std::true_type {};

bool require(bool condition, const char *message)
{
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}

bool throwsInvalidArgumentForInvalidFrameRate()
{
    try {
        mini_editor::playback_core::FrameRate invalid(0, 1);
        (void)invalid;
    } catch (const std::invalid_argument &) {
        return true;
    }

    return false;
}

bool verifyFrameBoundaries(mini_editor::playback_core::FrameRate rate)
{
    using namespace mini_editor::playback_core;

    const TimelineFrame frame = TimelineFrame::fromFrameNumber(123);
    const SequenceTime start = sequenceTimeAtFrameStart(frame, rate);
    const SequenceTime next = sequenceTimeAtNextFrameStart(frame, rate);

    return require(frameAtSequenceTime(start, rate) == frame,
                   "A frame start must resolve to its own frame.")
        && require(frameAtSequenceTime(next, rate)
                       == frame + FrameCount::fromFrames(1),
                   "The next frame start must honor the half-open boundary.")
        && require(start < next, "Every frame interval must have positive duration.");
}

bool verifyTwentyFourHourRange(mini_editor::playback_core::FrameRate rate)
{
    using namespace mini_editor::playback_core;

    const SequenceTime endOfDay = SequenceTime::fromMicroseconds(86'400'000'000);
    const TimelineFrame frame = frameAtSequenceTime(endOfDay, rate);
    const SequenceTime start = sequenceTimeAtFrameStart(frame, rate);
    const SequenceTime next = sequenceTimeAtNextFrameStart(frame, rate);

    return require(start <= endOfDay && endOfDay < next,
                   "A 24-hour value must remain inside its resolved frame interval.");
}

bool verifySourceMapping()
{
    using namespace mini_editor::playback_core;

    const ClipTimeMapping video {
        TimelineFrame::fromFrameNumber(100),
        SourceTimestamp::fromMicroseconds(0),
        FrameRate(30, 1)
    };
    const auto sourceAtStart = sourceTimestampFor(
        TimelineFrame::fromFrameNumber(100), video);
    const auto sourceOneSecondLater = sourceTimestampFor(
        TimelineFrame::fromFrameNumber(130), video);

    const ClipTimeMapping still {
        TimelineFrame::zero(),
        std::nullopt,
        FrameRate(30, 1)
    };

    return require(sourceAtStart && sourceAtStart->microsecondsForAdapter() == 0,
                   "A zero source timestamp must remain valid media time.")
        && require(sourceOneSecondLater
                       && sourceOneSecondLater->microsecondsForAdapter() == 1'000'000,
                   "Source mapping must use a named sequence-to-source conversion.")
        && require(!sourceTimestampFor(TimelineFrame::zero(), still),
                   "Still-image mapping must not invent a source timestamp.");
}

bool verifyRateConversions()
{
    using namespace mini_editor::playback_core;

    return require(sequenceElapsedFor(ClockDuration::fromMicroseconds(1'000'000), 200)
                       == SequenceDuration::fromMicroseconds(2'000'000),
                   "A 200% rate must double sequence elapsed time.")
        && require(clockElapsedFor(SequenceDuration::fromMicroseconds(2'000'000), 200)
                       == ClockDuration::fromMicroseconds(1'000'000),
                   "Clock elapsed time must use the named inverse rate boundary.")
        && require(sequenceElapsedFor(ClockDuration::fromMicroseconds(-1), 50)
                       == SequenceDuration::zero(),
                   "Signed sub-microsecond rate results must truncate toward zero.");
}

} // namespace

int main()
{
    using namespace mini_editor::playback_core;

    static_assert(CoreApiVersion::major == 1,
                  "The test documents the initial playback-core API level.");
    static_assert(!std::is_convertible<int, TimelineFrame>::value,
                  "Raw frame values must not implicitly become timeline frames.");
    static_assert(!std::is_convertible<TimelineFrame, SourceTimestamp>::value,
                  "Timeline and source time must remain separate domains.");
    static_assert(!HasAddition<SequenceTime, ClockDuration>::value,
                  "Clock duration must not be added directly to sequence time.");
    static_assert(!HasSubtraction<SourceTimestamp, TimelineFrame>::value,
                  "Source timestamps and timeline frames have no shared arithmetic.");

    if (!hasExpectedCoreApiVersion()) {
        std::cerr << "Playback core API version is not supported.\n";
        return 1;
    }

    if (!require(throwsInvalidArgumentForInvalidFrameRate(),
                 "FrameRate must reject nonpositive values.")
        || !require(FrameRate(30, 1) == FrameRate(60, 2),
                    "FrameRate must normalize rationally equal values.")
        || !require(TimelineFrame::fromFrameNumber(12)
                        - TimelineFrame::fromFrameNumber(7)
                        == FrameCount::fromFrames(5),
                    "Timeline-frame subtraction must return a frame count.")
        || !require(TimelineFrame::fromFrameNumber(7)
                        + FrameCount::fromFrames(5)
                        == TimelineFrame::fromFrameNumber(12),
                    "Timeline-frame addition must use a frame count.")
        || !verifyFrameBoundaries(FrameRate(24, 1))
        || !verifyFrameBoundaries(FrameRate(25, 1))
        || !verifyFrameBoundaries(FrameRate(30, 1))
        || !verifyFrameBoundaries(FrameRate(30'000, 1'001))
        || !verifyTwentyFourHourRange(FrameRate(24, 1))
        || !verifyTwentyFourHourRange(FrameRate(25, 1))
        || !verifyTwentyFourHourRange(FrameRate(30, 1))
        || !verifyTwentyFourHourRange(FrameRate(30'000, 1'001))
        || !verifySourceMapping()
        || !verifyRateConversions()) {
        return 1;
    }

    return 0;
}
