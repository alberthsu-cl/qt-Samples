#include "PlaybackClockController.h"

#include "EditorSession.h"

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
