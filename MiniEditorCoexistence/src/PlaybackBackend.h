#pragma once

#include "PlaybackClockController.h"
#include "PreviewSeekRequest.h"

class EditorSession;
enum class LegacyPlaybackCommand;

// A playback backend translates transport intent into editor state and tells
// its host whether a periodic UI tick is currently required. The simulated
// backend below uses EditorSession as its clock source; a future media backend
// can implement the same boundary with QMediaPlayer callbacks instead.
class IPlaybackBackend
{
public:
    virtual ~IPlaybackBackend() = default;

    virtual unsigned int tickIntervalMilliseconds() const = 0;
    virtual PlaybackClockAction executeCommand(LegacyPlaybackCommand command) = 0;
    virtual PlaybackClockAction seek(const PreviewSeekRequest &request) = 0;
    virtual PlaybackClockAction synchronize() = 0;
    virtual PlaybackClockAction advanceOneFrame() = 0;
};

// The learning sample's initial backend. It deliberately retains the original
// frame-by-frame timer behavior while isolating it from MFC and Qt UI code.
class SimulatedPlaybackBackend final : public IPlaybackBackend
{
public:
    explicit SimulatedPlaybackBackend(EditorSession &session);

    unsigned int tickIntervalMilliseconds() const override;
    PlaybackClockAction executeCommand(LegacyPlaybackCommand command) override;
    PlaybackClockAction seek(const PreviewSeekRequest &request) override;
    PlaybackClockAction synchronize() override;
    PlaybackClockAction advanceOneFrame() override;

private:
    EditorSession &session_;
    PlaybackClockController clock_;
};
