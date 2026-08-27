#pragma once

class EditorSession;

// Framework-neutral policy for a UI timer that advances the editor playhead.
// MFC's SetTimer, Qt's QTimer, or a future media-engine clock can use this
// without moving playback rules into the UI layer.
enum class PlaybackClockAction {
    EnsureRunning,
    Stop
};

class PlaybackClockController final
{
public:
    static constexpr unsigned int kTickIntervalMilliseconds = 33;

    explicit PlaybackClockController(EditorSession &session);

    PlaybackClockAction synchronize() const;
    PlaybackClockAction advanceOneFrame();

private:
    EditorSession &session_;
};
