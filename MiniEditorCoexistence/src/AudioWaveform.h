#pragma once

#include <vector>

struct AudioWaveformPeak {
    float minimum = 0.0F;
    float maximum = 0.0F;
};

struct AudioWaveformData {
    int sampleRate = 0;
    int sampleFramesPerPeak = 256;
    std::vector<AudioWaveformPeak> peaks;
};
