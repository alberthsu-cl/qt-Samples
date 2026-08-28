#pragma once

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

// Human-readable project time at a fixed editor frame rate. Minutes are not
// wrapped at 60, so long media remains unambiguous (for example 72:15:12).
inline std::wstring frameTimecodeMmSsFf(int frame, int framesPerSecond = 30)
{
    frame = std::max(0, frame);
    framesPerSecond = std::max(1, framesPerSecond);
    const int framePart = frame % framesPerSecond;
    const int totalSeconds = frame / framesPerSecond;

    std::wostringstream text;
    text << std::setfill(L'0')
         << std::setw(2) << totalSeconds / 60 << L':'
         << std::setw(2) << totalSeconds % 60 << L':'
         << std::setw(2) << framePart;
    return text.str();
}
