#include "PlaybackBackend.h"

#include "EditorSession.h"

SimulatedPlaybackBackend::SimulatedPlaybackBackend(EditorSession &session)
    : session_(session)
    , clock_(session)
{
}

unsigned int SimulatedPlaybackBackend::tickIntervalMilliseconds() const
{
    return PlaybackClockController::kTickIntervalMilliseconds;
}

PlaybackClockAction SimulatedPlaybackBackend::executeCommand(
    PlaybackCommand command)
{
    session_.handlePlaybackCommand(command);
    return synchronize();
}

PlaybackClockAction SimulatedPlaybackBackend::synchronize()
{
    return clock_.synchronize();
}

PlaybackClockAction SimulatedPlaybackBackend::seek(
    const PreviewSeekRequest &)
{
    return synchronize();
}

PlaybackClockAction SimulatedPlaybackBackend::advanceOneFrame()
{
    return clock_.advanceOneFrame();
}
