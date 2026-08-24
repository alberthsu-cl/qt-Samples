#include "TimelineModel.h"

#include <algorithm>

const std::vector<TimelineClip> &TimelineModel::clips() const
{
    return clips_;
}

int TimelineModel::addClip(int mediaAssetIndex, const TimelineClipState &state)
{
    const int clipId = nextClipId_++;
    clips_.push_back({ clipId, mediaAssetIndex, state });
    return clipId;
}

bool TimelineModel::moveClip(int clipId, const TimelineClipState &state)
{
    const auto iterator = std::find_if(clips_.begin(), clips_.end(),
        [clipId](const TimelineClip &clip) { return clip.id == clipId; });
    if (iterator == clips_.end())
        return false;

    iterator->state = state;
    return true;
}

bool TimelineModel::removeClip(int clipId)
{
    const auto iterator = std::find_if(clips_.begin(), clips_.end(),
        [clipId](const TimelineClip &clip) { return clip.id == clipId; });
    if (iterator == clips_.end())
        return false;

    clips_.erase(iterator);
    return true;
}

const TimelineClip *TimelineModel::findClip(int clipId) const
{
    const auto iterator = std::find_if(clips_.begin(), clips_.end(),
        [clipId](const TimelineClip &clip) { return clip.id == clipId; });
    return iterator == clips_.end() ? nullptr : &*iterator;
}
