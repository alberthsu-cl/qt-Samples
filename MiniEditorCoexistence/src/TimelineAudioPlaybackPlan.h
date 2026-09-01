#pragma once

class EditorSession;
class MediaLibrary;

// Framework-neutral decoder intent for the single A1 lane. It is separate
// from MediaPlaybackPlan because V1 video and A1 audio can be active at the
// same timeline frame and therefore need independent players.
struct TimelineAudioPlaybackPlan {
    int mediaAssetId = 0;
    int timelineClipId = 0;
    int sourceFrame = 0;
    int sourceDurationFrames = 0;
    int fadeGainPercent = 100;
    bool shouldPlay = false;
    bool isPaused = false;

    bool hasAudio() const;
};

class TimelineAudioPlaybackPlanResolver final
{
public:
    static TimelineAudioPlaybackPlan resolve(
        const EditorSession &session,
        const MediaLibrary &mediaLibrary);
};
