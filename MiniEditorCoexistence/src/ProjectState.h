#pragma once

// Framework-neutral editable state for the clip currently selected in the
// mini editor. Both MFC and Qt use this value type without owning each other.
enum class ClipPosition {
    Center,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

struct ClipSettings {
    int opacityPercent = 100;
    int scalePercent = 100;
    ClipPosition position = ClipPosition::Center;
};

// Playback remains application/MFC-owned during this migration. Qt transport
// controls send commands and display this state; they do not own a player.
struct PlaybackState {
    bool isPlaying = false;
    int currentFrame = 0;
    int framesPerSecond = 30;
};

enum class PlaybackCommand {
    TogglePlayPause,
    Stop,
    StepBackward,
    StepForward
};

// Framework-neutral view state for the timeline. MainFrame owns it; either
// an MFC or Qt control may present/edit it during the migration.
struct TimelineViewState {
    int zoomPercent = 100;
    bool isAudioTrackVisible = true;
};

inline const wchar_t *clipPositionDisplayName(ClipPosition position)
{
    switch (position) {
    case ClipPosition::Center:
        return L"Center";
    case ClipPosition::TopLeft:
        return L"Top Left";
    case ClipPosition::TopRight:
        return L"Top Right";
    case ClipPosition::BottomLeft:
        return L"Bottom Left";
    case ClipPosition::BottomRight:
        return L"Bottom Right";
    }

    return L"Unknown";
}
