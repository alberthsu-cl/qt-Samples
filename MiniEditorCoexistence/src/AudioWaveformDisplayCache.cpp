#include "AudioWaveformDisplayCache.h"

#include <algorithm>
#include <functional>
#include <utility>

bool AudioWaveformDisplayKey::operator==(
    const AudioWaveformDisplayKey &other) const
{
    return mediaAssetId == other.mediaAssetId
        && sourceInFrame == other.sourceInFrame
        && durationFrames == other.durationFrames
        && pixelWidth == other.pixelWidth
        && timelineFramesPerSecond == other.timelineFramesPerSecond;
}

AudioWaveformDisplayCache::AudioWaveformDisplayCache(std::size_t capacity)
    : capacity_(std::max<std::size_t>(1, capacity))
{
}

SharedAudioWaveform AudioWaveformDisplayCache::find(
    const AudioWaveformDisplayKey &key)
{
    const auto found = entries_.find(key);
    if (found == entries_.end())
        return {};

    mostRecentlyUsed_.splice(mostRecentlyUsed_.begin(), mostRecentlyUsed_,
                             found->second);
    return found->second->waveform;
}

SharedAudioWaveform AudioWaveformDisplayCache::store(
    const AudioWaveformDisplayKey &key,
    std::vector<AudioWaveformPeak> peaks)
{
    if (const auto found = entries_.find(key); found != entries_.end()) {
        found->second->waveform =
            std::make_shared<const std::vector<AudioWaveformPeak>>(
                std::move(peaks));
        mostRecentlyUsed_.splice(mostRecentlyUsed_.begin(), mostRecentlyUsed_,
                                 found->second);
        return found->second->waveform;
    }

    while (entries_.size() >= capacity_) {
        entries_.erase(mostRecentlyUsed_.back().key);
        mostRecentlyUsed_.pop_back();
    }

    mostRecentlyUsed_.push_front({
        key,
        std::make_shared<const std::vector<AudioWaveformPeak>>(
            std::move(peaks))
    });
    entries_.insert({ key, mostRecentlyUsed_.begin() });
    return mostRecentlyUsed_.front().waveform;
}

void AudioWaveformDisplayCache::invalidateMediaAsset(int mediaAssetId)
{
    for (auto entry = mostRecentlyUsed_.begin();
         entry != mostRecentlyUsed_.end();) {
        if (entry->key.mediaAssetId != mediaAssetId) {
            ++entry;
            continue;
        }
        entries_.erase(entry->key);
        entry = mostRecentlyUsed_.erase(entry);
    }
}

std::size_t AudioWaveformDisplayCache::size() const
{
    return entries_.size();
}

std::size_t AudioWaveformDisplayCache::KeyHash::operator()(
    const AudioWaveformDisplayKey &key) const
{
    std::size_t value = 0;
    const auto combine = [&value](int field) {
        value ^= std::hash<int>{}(field) + 0x9e3779b9U
            + (value << 6U) + (value >> 2U);
    };
    combine(key.mediaAssetId);
    combine(key.sourceInFrame);
    combine(key.durationFrames);
    combine(key.pixelWidth);
    combine(key.timelineFramesPerSecond);
    return value;
}
