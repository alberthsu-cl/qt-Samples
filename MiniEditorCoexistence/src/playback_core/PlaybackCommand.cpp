#include "PlaybackCommand.h"

#include <algorithm>
#include <atomic>

namespace mini_editor::playback_core {
namespace {

std::atomic<std::uint64_t> nextSessionId { 1 };
std::atomic<std::uint64_t> nextCommandId { 1 };

} // namespace

MediaAssetId::MediaAssetId(int value) : value_(value) {}
int MediaAssetId::value() const { return value_; }
bool operator==(MediaAssetId left, MediaAssetId right) { return left.value() == right.value(); }
bool operator!=(MediaAssetId left, MediaAssetId right) { return !(left == right); }

PlaybackSessionId::PlaybackSessionId(std::uint64_t value) : value_(value) {}
PlaybackSessionId PlaybackSessionId::create()
{
    return PlaybackSessionId(nextSessionId.fetch_add(1));
}
std::uint64_t PlaybackSessionId::valueForDiagnostics() const { return value_; }
bool operator==(PlaybackSessionId left, PlaybackSessionId right)
{
    return left.valueForDiagnostics() == right.valueForDiagnostics();
}
bool operator!=(PlaybackSessionId left, PlaybackSessionId right) { return !(left == right); }

PlaybackGeneration::PlaybackGeneration(std::uint64_t value) : value_(value) {}
PlaybackGeneration PlaybackGeneration::initial() { return PlaybackGeneration(0); }
std::uint64_t PlaybackGeneration::value() const { return value_; }
PlaybackGeneration PlaybackGeneration::next() const { return PlaybackGeneration(value_ + 1); }
bool operator==(PlaybackGeneration left, PlaybackGeneration right)
{
    return left.value() == right.value();
}
bool operator!=(PlaybackGeneration left, PlaybackGeneration right) { return !(left == right); }

PlaybackCommandId::PlaybackCommandId(std::uint64_t value) : value_(value) {}
PlaybackCommandId PlaybackCommandId::create()
{
    return PlaybackCommandId(nextCommandId.fetch_add(1));
}
std::uint64_t PlaybackCommandId::valueForDiagnostics() const { return value_; }
bool operator==(PlaybackCommandId left, PlaybackCommandId right)
{
    return left.valueForDiagnostics() == right.valueForDiagnostics();
}
bool operator!=(PlaybackCommandId left, PlaybackCommandId right) { return !(left == right); }

StatusSequenceNumber::StatusSequenceNumber(std::uint64_t value) : value_(value) {}
StatusSequenceNumber StatusSequenceNumber::initial() { return StatusSequenceNumber(0); }
std::uint64_t StatusSequenceNumber::value() const { return value_; }
StatusSequenceNumber StatusSequenceNumber::next() const { return StatusSequenceNumber(value_ + 1); }
bool operator==(StatusSequenceNumber left, StatusSequenceNumber right)
{
    return left.value() == right.value();
}
bool operator!=(StatusSequenceNumber left, StatusSequenceNumber right) { return !(left == right); }
bool operator<(StatusSequenceNumber left, StatusSequenceNumber right)
{
    return left.value() < right.value();
}

SourceTimestamp sourceTimeZero()
{
    return SourceTimestamp::fromMicroseconds(0);
}

int sourceProgressPermille(SourceTimestamp position, SourceTimestamp end)
{
    const std::int64_t endUs = end.microsecondsForAdapter();
    if (end == sourceTimeZero() || endUs <= 0)
        return 0;

    const std::int64_t positionUs = position.microsecondsForAdapter();
    const std::int64_t permille = (positionUs * 1000) / endUs;
    return static_cast<int>(std::clamp<std::int64_t>(permille, 0, 1000));
}

} // namespace mini_editor::playback_core
