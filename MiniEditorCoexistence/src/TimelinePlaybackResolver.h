#pragma once

#include "MediaLibrary.h"
#include "TimelineModel.h"

#include <optional>

// One source-media sample needed to render a timeline frame. The resolver
// translates editor time into clip-local and source-media time without
// depending on either MFC or Qt.
struct ResolvedTimelineMedia {
    int clipId = 0;
    int mediaAssetId = 0;
    MediaKind mediaKind = MediaKind::Video;
    int clipLocalFrame = 0;
    int sourceFrame = 0;
    int sourceDurationFrames = 0;
    ClipSettings settings;
};

struct ResolvedTimelineFrame {
    int timelineFrame = 0;
    std::optional<ResolvedTimelineMedia> video;
    std::optional<ResolvedTimelineMedia> audio;
};

class TimelinePlaybackResolver final
{
public:
    static ResolvedTimelineFrame resolve(const TimelineModel &timeline,
                                         const MediaLibrary &mediaLibrary,
                                         int timelineFrame);

    static std::optional<ResolvedTimelineMedia> resolveClip(
        const TimelineClip &clip,
        const MediaLibrary &mediaLibrary,
        int timelineFrame);
};
