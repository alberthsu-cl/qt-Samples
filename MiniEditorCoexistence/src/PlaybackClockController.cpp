#include "PlaybackClockController.h"

#include "EditorSession.h"

#include <algorithm>

unsigned int PlaybackClockController::tickIntervalMillisecondsForRate(
    int ratePercent)
{
    const unsigned int safeRate = static_cast<unsigned int>(
        std::clamp(ratePercent, 50, 200));
    return std::max(1U, (kTickIntervalMilliseconds * 100U
                        + safeRate / 2U) / safeRate);
}

PlaybackClockController::PlaybackClockController(EditorSession &session)
    : session_(session)
{
}

PlaybackClockAction PlaybackClockController::synchronize() const
{
    return session_.playbackState().isPlaying
        ? PlaybackClockAction::EnsureRunning
        : PlaybackClockAction::Stop;
}

PlaybackClockAction PlaybackClockController::advanceOneFrame()
{
    if (session_.playbackState().isPlaying)
        session_.advancePlaybackFrame();

    return synchronize();
}
