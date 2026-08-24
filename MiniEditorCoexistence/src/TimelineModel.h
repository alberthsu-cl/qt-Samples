#pragma once

#include "ProjectState.h"

#include <cstddef>
#include <vector>

// One placement of a media asset on the timeline. A source asset may appear
// more than once, so a timeline clip has its own identity and timing.
struct TimelineClip {
    int id = 0;
    int mediaAssetIndex = 0;
    TimelineClipState state;
};

// Framework-neutral timeline collection. It starts empty; UI code can add
// clips through this API without knowing whether the view is MFC or Qt.
class TimelineModel final
{
public:
    const std::vector<TimelineClip> &clips() const;

    int addClip(int mediaAssetIndex,
                const TimelineClipState &state = {});
    bool moveClip(int clipId, const TimelineClipState &state);
    bool removeClip(int clipId);
    const TimelineClip *findClip(int clipId) const;

private:
    std::vector<TimelineClip> clips_;
    int nextClipId_ = 1;
};
