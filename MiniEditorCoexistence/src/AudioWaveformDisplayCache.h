#pragma once

#include "AudioWaveform.h"

#include <cstddef>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

struct AudioWaveformDisplayKey {
    int mediaAssetId = 0;
    int sourceInFrame = 0;
    int durationFrames = 0;
    int pixelWidth = 0;
    int timelineFramesPerSecond = 30;

    bool operator==(const AudioWaveformDisplayKey &other) const;
};

using SharedAudioWaveform =
    std::shared_ptr<const std::vector<AudioWaveformPeak>>;

// A small LRU cache for display-ready waveform columns. Position on the
// timeline is intentionally absent from the key: moving a clip does not
// change which part of its source is shown. Trim, width/zoom, and FPS do.
class AudioWaveformDisplayCache final
{
public:
    explicit AudioWaveformDisplayCache(std::size_t capacity = 96);

    SharedAudioWaveform find(const AudioWaveformDisplayKey &key);
    SharedAudioWaveform store(const AudioWaveformDisplayKey &key,
                              std::vector<AudioWaveformPeak> peaks);
    void invalidateMediaAsset(int mediaAssetId);
    std::size_t size() const;

private:
    struct KeyHash {
        std::size_t operator()(const AudioWaveformDisplayKey &key) const;
    };
    struct Entry {
        AudioWaveformDisplayKey key;
        SharedAudioWaveform waveform;
    };

    using EntryList = std::list<Entry>;
    using EntryIterator = EntryList::iterator;

    std::size_t capacity_ = 96;
    EntryList mostRecentlyUsed_;
    std::unordered_map<AudioWaveformDisplayKey, EntryIterator, KeyHash> entries_;
};
