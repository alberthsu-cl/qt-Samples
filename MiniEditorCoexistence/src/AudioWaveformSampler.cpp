#include "AudioWaveformSampler.h"

#include <algorithm>
#include <cstdint>

std::vector<AudioWaveformPeak> AudioWaveformSampler::sample(
    const AudioWaveformData &waveform,
    int sourceInFrame,
    int durationFrames,
    int pixelWidth,
    int timelineFramesPerSecond)
{
    if (waveform.sampleRate <= 0 || waveform.sampleFramesPerPeak <= 0
        || waveform.peaks.empty() || durationFrames <= 0 || pixelWidth <= 0) {
        return {};
    }

    pixelWidth = std::min(pixelWidth, 8192);
    timelineFramesPerSecond = std::max(1, timelineFramesPerSecond);
    sourceInFrame = std::max(0, sourceInFrame);

    std::vector<AudioWaveformPeak> result(pixelWidth);
    for (int pixel = 0; pixel < pixelWidth; ++pixel) {
        const std::int64_t firstTimelineFrame = sourceInFrame
            + static_cast<std::int64_t>(durationFrames) * pixel / pixelWidth;
        const std::int64_t lastTimelineFrame = sourceInFrame
            + static_cast<std::int64_t>(durationFrames) * (pixel + 1)
                / pixelWidth;
        const std::int64_t firstSampleFrame = firstTimelineFrame
            * waveform.sampleRate / timelineFramesPerSecond;
        const std::int64_t lastSampleFrame = std::max(
            firstSampleFrame + 1,
            lastTimelineFrame * waveform.sampleRate
                / timelineFramesPerSecond);
        const int firstPeak = std::clamp(
            static_cast<int>(firstSampleFrame / waveform.sampleFramesPerPeak),
            0, static_cast<int>(waveform.peaks.size()) - 1);
        const int lastPeak = std::clamp(
            static_cast<int>((lastSampleFrame - 1)
                             / waveform.sampleFramesPerPeak),
            firstPeak, static_cast<int>(waveform.peaks.size()) - 1);

        AudioWaveformPeak aggregate = waveform.peaks[firstPeak];
        for (int peak = firstPeak + 1; peak <= lastPeak; ++peak) {
            aggregate.minimum = std::min(aggregate.minimum,
                                         waveform.peaks[peak].minimum);
            aggregate.maximum = std::max(aggregate.maximum,
                                         waveform.peaks[peak].maximum);
        }
        result[pixel] = aggregate;
    }
    return result;
}
