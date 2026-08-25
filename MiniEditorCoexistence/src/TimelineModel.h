#pragma once

#include "ProjectState.h"

#include <cstddef>
#include <vector>

enum class TimelineTrackType {
    Video,
    Audio
};

// One placement of a media asset on the timeline. A source asset may appear
// more than once, so a timeline clip has its own identity and timing.
struct TimelineClip {
    int id = 0;
    int mediaAssetId = 0;
    TimelineTrackType trackType = TimelineTrackType::Video;
    TimelineClipState state;
    // Placement properties belong to this timeline instance, never to the
    // reusable source asset in MediaLibrary.
    ClipSettings settings;
};

// Framework-neutral timeline collection. It starts empty; UI code can add
// clips through this API without knowing whether the view is MFC or Qt.
class TimelineModel final
{
public:
    static constexpr int kMinimumDurationFrames = 600;

    const std::vector<TimelineClip> &clips() const;
    int contentDurationFrames() const;
    int durationFrames() const;

    int addClip(int mediaAssetId,
                TimelineTrackType trackType,
                const TimelineClipState &state = {});
    bool restoreClip(const TimelineClip &clip);
    bool moveClip(int clipId, const TimelineClipState &state);
    bool updateClipSettings(int clipId, const ClipSettings &settings);
    bool removeClip(int clipId);
    void clear();
    const TimelineClip *findClip(int clipId) const;
    const TimelineClip *visibleVideoClipAt(int frame) const;

private:
    std::vector<TimelineClip> clips_;
    int nextClipId_ = 1;
};
