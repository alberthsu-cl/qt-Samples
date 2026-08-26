#include "TimelinePlaybackResolver.h"

#include <algorithm>

std::optional<ResolvedTimelineMedia> TimelinePlaybackResolver::resolveClip(
    const TimelineClip &clip, const MediaLibrary &mediaLibrary, int timelineFrame)
{
    const int clipEnd = clip.state.startFrame + clip.state.durationFrames;
    if (timelineFrame < clip.state.startFrame || timelineFrame >= clipEnd)
        return std::nullopt;

    const LibraryMediaAsset *asset = mediaLibrary.findAsset(clip.mediaAssetId);
    if (asset == nullptr)
        return std::nullopt;

    const int clipLocalFrame = timelineFrame - clip.state.startFrame;
    const int sourceFrame = asset->kind == MediaKind::Image
        ? 0 : clip.state.sourceInFrame + clipLocalFrame;
    return ResolvedTimelineMedia{
        clip.id,
        clip.mediaAssetId,
        asset->kind,
        clipLocalFrame,
        sourceFrame,
        asset->timelineDurationFrames,
        clip.settings
    };
}

ResolvedTimelineFrame TimelinePlaybackResolver::resolve(
    const TimelineModel &timeline, const MediaLibrary &mediaLibrary,
    int timelineFrame)
{
    ResolvedTimelineFrame result;
    result.timelineFrame = std::max(0, timelineFrame);

    for (const TimelineClip &clip : timeline.clips()) {
        const std::optional<ResolvedTimelineMedia> media = resolveClip(
            clip, mediaLibrary, result.timelineFrame);
        if (!media)
            continue;

        if (clip.trackType == TimelineTrackType::Video)
            result.video = media;
        else
            result.audio = media;
    }
    return result;
}
