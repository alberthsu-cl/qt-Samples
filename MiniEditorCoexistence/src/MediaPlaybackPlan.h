#pragma once

#include "MediaKind.h"

class EditorSession;
class MediaLibrary;

// Identifies which editor context owns the media decoder at this moment.
enum class MediaPlaybackContext {
    None,
    Source,
    Timeline
};

// A framework-neutral description of the media frame the editor wants.
// It contains editor intent only: QtMediaPlaybackBackend remains responsible
// for checking files and translating frames into QMediaPlayer positions.
struct MediaPlaybackPlan {
    MediaPlaybackContext context = MediaPlaybackContext::None;
    int mediaAssetId = 0;
    int timelineClipId = 0;
    MediaKind mediaKind = MediaKind::Video;
    int sourceFrame = 0;
    int sourceDurationFrames = 0;
    bool shouldPlay = false;
    bool isPaused = false;

    bool hasMedia() const;
    bool usesMediaDecoder() const;
    bool needsSilentVideoPreroll() const;
};

// Converts EditorSession state into one desired decoder state. Keeping this
// policy outside Qt makes source/timeline switching and trimmed-source seeking
// directly testable without constructing QMediaPlayer.
class MediaPlaybackPlanResolver final
{
public:
    static MediaPlaybackPlan resolve(const EditorSession &session,
                                     const MediaLibrary &mediaLibrary);
};
