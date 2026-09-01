#pragma once

#include "AudioWaveform.h"

#include <vector>

// Converts decoder-oriented PCM peak blocks into one min/max column per
// visible timeline pixel. This policy is pure C++: Qt decodes the audio, but
// trim and zoom mapping remain testable without a UI framework.
class AudioWaveformSampler final
{
public:
    static std::vector<AudioWaveformPeak> sample(
        const AudioWaveformData &waveform,
        int sourceInFrame,
        int durationFrames,
        int pixelWidth,
        int timelineFramesPerSecond = 30);
};
