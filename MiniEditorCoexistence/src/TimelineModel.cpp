#include "TimelineModel.h"

#include "ClipFade.h"
#include "TimelineTrackPolicy.h"

#include <algorithm>
#include <utility>

const std::vector<TimelineClip> &TimelineModel::clips() const
{
    return clips_;
}

int TimelineModel::contentDurationFrames() const
{
    int duration = 0;
    for (const TimelineClip &clip : clips_)
        duration = std::max(duration, clip.state.startFrame + clip.state.durationFrames);
    return duration;
}

int TimelineModel::durationFrames() const
{
    return std::max(kMinimumDurationFrames, contentDurationFrames());
}

int TimelineModel::addClip(int mediaAssetId, TimelineTrackType trackType,
                           const TimelineClipState &state)
{
    if (!TimelineTrackPolicy::canPlace(clips_, trackType, state))
        return 0;

    const int clipId = nextClipId_++;
    clips_.push_back({ clipId, mediaAssetId, trackType, state, {} });
    return clipId;
}

bool TimelineModel::updateClipSettings(int clipId, const ClipSettings &settings)
{
    auto iterator = std::find_if(clips_.begin(), clips_.end(),
        [clipId](const TimelineClip &clip) { return clip.id == clipId; });
    if (iterator == clips_.end())
        return false;
    iterator->settings = settings;
    return true;
}

bool TimelineModel::restoreClip(const TimelineClip &clip)
{
    if (clip.id <= 0 || findClip(clip.id) != nullptr
        || !TimelineTrackPolicy::canPlace(clips_, clip.trackType, clip.state))
        return false;

    clips_.push_back(clip);
    nextClipId_ = std::max(nextClipId_, clip.id + 1);
    return true;
}

bool TimelineModel::moveClip(int clipId, const TimelineClipState &state)
{
    const auto iterator = std::find_if(clips_.begin(), clips_.end(),
        [clipId](const TimelineClip &clip) { return clip.id == clipId; });
    if (iterator == clips_.end())
        return false;

    if (!TimelineTrackPolicy::canPlace(clips_, iterator->trackType, state, clipId))
        return false;

    iterator->state = state;
    return true;
}

int TimelineModel::splitClip(int clipId, int splitFrame, MediaKind mediaKind)
{
    const auto iterator = std::find_if(clips_.begin(), clips_.end(),
        [clipId](const TimelineClip &clip) { return clip.id == clipId; });
    if (iterator == clips_.end())
        return 0;

    const int clipStart = iterator->state.startFrame;
    const int clipEnd = clipStart + iterator->state.durationFrames;
    if (splitFrame <= clipStart || splitFrame >= clipEnd)
        return 0;

    const int leftDuration = splitFrame - clipStart;
    TimelineClip rightClip = *iterator;
    rightClip.id = nextClipId_++;
    rightClip.state.startFrame = splitFrame;
    rightClip.state.durationFrames = clipEnd - splitFrame;
    rightClip.state.sourceInFrame = mediaKind == MediaKind::Image
        ? 0 : iterator->state.sourceInFrame + leftDuration;

    // A fade belongs to an edge of the original clip, so each half keeps only
    // the ramp it still owns. The new cut point itself is not a fade.
    rightClip.settings.fadeInFrames = 0;
    rightClip.settings = ClipFade::clampSettings(rightClip.settings,
                                                 rightClip.state.durationFrames);

    iterator->state.durationFrames = leftDuration;
    if (mediaKind == MediaKind::Image)
        iterator->state.sourceInFrame = 0;
    iterator->settings.fadeOutFrames = 0;
    iterator->settings = ClipFade::clampSettings(iterator->settings, leftDuration);
    clips_.push_back(rightClip);
    return rightClip.id;
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

bool TimelineModel::replaceClips(const std::vector<TimelineClip> &clips)
{
    TimelineModel replacement;
    for (const TimelineClip &clip : clips) {
        if (!replacement.restoreClip(clip))
            return false;
    }

    *this = std::move(replacement);
    return true;
}

void TimelineModel::clear()
{
    clips_.clear();
    nextClipId_ = 1;
}

const TimelineClip *TimelineModel::findClip(int clipId) const
{
    const auto iterator = std::find_if(clips_.begin(), clips_.end(),
        [clipId](const TimelineClip &clip) { return clip.id == clipId; });
    return iterator == clips_.end() ? nullptr : &*iterator;
}
