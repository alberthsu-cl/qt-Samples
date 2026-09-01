#pragma once

#include "ClipEffect.h"
#include "MediaKind.h"

#include <cstdint>
#include <string>

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
    // Fade lengths in frames, measured from each end of the placement. They
    // are edit decisions, so they travel with the project and take part in
    // Undo/Redo; ClipFade owns the rules that keep them valid.
    int fadeInFrames = 0;
    int fadeOutFrames = 0;
    // DSP belongs to the timeline placement, just like opacity and fading.
    // Keeping it here makes each clip independently editable, undoable, and
    // serializable while source-library preview remains untouched.
    ClipEffectKind effect = ClipEffectKind::None;
    int effectIntensityPercent = 100;
};

// Playback remains application/MFC-owned during this migration. Qt transport
// controls send commands and display this state; they do not own a player.
struct PlaybackState {
    bool isPlaying = false;
    // Paused is distinct from stopped: both have isPlaying == false, but a
    // paused preview must keep showing the exact frame under the playhead.
    bool isPaused = false;
    int currentFrame = 0;
    int framesPerSecond = 30;
    int durationFrames = 300;
    // Preview-only transport rate. This is deliberately transient: changing
    // it does not retime clips or alter the saved project.
    int playbackRatePercent = 100;
};

enum class PreviewMode {
    Source,
    Timeline
};

struct PreviewState {
    PreviewMode mode = PreviewMode::Source;
    bool hasMedia = false;
    int mediaAssetId = 0;
    std::wstring displayName;
    std::uint32_t thumbnailColorRgb = 0;
    ClipSettings settings;
    // settings.opacityPercent modulated by the video clip's fade ramp. The
    // renderer uses this value; the panel still reports the stored opacity.
    int effectiveOpacityPercent = 100;
    int videoFadeGainPercent = 100;
    int audioFadeGainPercent = 100;
    MediaKind mediaKind = MediaKind::Video;
    int timelineFrame = 0;
    int clipLocalFrame = 0;
    int sourceFrame = 0;
    int sourceDurationFrames = 0;
    bool hasAudio = false;
    std::wstring audioDisplayName;
    int audioSourceFrame = 0;
    int audioSourceDurationFrames = 0;
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
    bool isRippleEditingEnabled = false;
};

// Project-owned audio routing for the timeline. Unlike TimelineViewState,
// this changes the produced mix and must therefore be saved in the project.
struct TimelineAudioMixState {
    bool isVideoTrackMuted = false;
};

// Project-edit state for one clip on the timeline. Unlike TimelineViewState,
// this is part of the edit decision and therefore belongs in Undo/Redo.
struct TimelineClipState {
    int startFrame = 0;
    int durationFrames = 180;
    // Video/audio: first source frame used by this placement. Still images
    // have no running source and therefore keep this value at zero.
    int sourceInFrame = 0;
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
