#include "SnapshotTimelineResolver.h"

#include <algorithm>

namespace mini_editor::playback_core {
namespace {

const PlaybackMediaDescriptor *findMedia(const SequencePlaybackSnapshot &snapshot,
                                         int mediaAssetId)
{
    const auto match = std::find_if(
        snapshot.media.begin(), snapshot.media.end(),
        [mediaAssetId](const PlaybackMediaDescriptor &descriptor) {
            return descriptor.mediaAssetId == mediaAssetId;
        });
    return match == snapshot.media.end() ? nullptr : &*match;
}

const std::vector<PlaybackClip> &trackClips(const SequencePlaybackSnapshot &snapshot,
                                            PlaybackTrackType trackType)
{
    return trackType == PlaybackTrackType::Audio ? snapshot.audioClips
                                                 : snapshot.videoClips;
}

bool covers(const PlaybackClip &clip, TimelineFrame frame)
{
    return frame >= clip.startFrame && frame < clip.startFrame + clip.duration;
}

} // namespace

std::optional<ResolvedSnapshotClip> SnapshotTimelineResolver::resolveTrack(
    const SequencePlaybackSnapshot &snapshot, PlaybackTrackType trackType,
    TimelineFrame timelineFrame)
{
    // A linear scan rather than a binary search over the builder's sorted,
    // non-overlapping tracks: the resolver must give a defined answer for any
    // snapshot it is handed, including one a test assembled by hand. When two
    // clips do overlap, the later one wins, which is what the legacy resolver
    // does when it overwrites its per-track result.
    const PlaybackClip *found = nullptr;
    for (const PlaybackClip &clip : trackClips(snapshot, trackType)) {
        if (covers(clip, timelineFrame))
            found = &clip;
    }
    if (found == nullptr)
        return std::nullopt;

    // The snapshot builder guarantees a descriptor for every referenced
    // asset. Treating its absence as "nothing on this track" keeps a
    // malformed snapshot from resolving to a clip whose media cannot be
    // opened.
    const PlaybackMediaDescriptor *media = findMedia(snapshot, found->mediaAssetId);
    if (media == nullptr)
        return std::nullopt;

    return ResolvedSnapshotClip {
        found->clipId,
        MediaAssetId(found->mediaAssetId),
        media->mediaKind,
        media->availability,
        media->immutableSourceLocator,
        timelineFrame - found->startFrame,
        found->duration,
        sourceTimestampFor(timelineFrame,
                           ClipTimeMapping { found->startFrame, found->sourceIn,
                                             snapshot.frameRate }),
        found->settings,
        // The same policy the legacy path uses for opacity, so the two cannot
        // disagree about the ramp at a given frame.
        clipFadeGainPercentAt(found->settings.fadeInFrames,
                              found->settings.fadeOutFrames,
                              static_cast<int>((timelineFrame - found->startFrame).frames()),
                              static_cast<int>(found->duration.frames()))
    };
}

ResolvedSnapshotFrame SnapshotTimelineResolver::resolve(
    const SequencePlaybackSnapshot &snapshot, TimelineFrame timelineFrame)
{
    return ResolvedSnapshotFrame {
        timelineFrame,
        resolveTrack(snapshot, PlaybackTrackType::Video, timelineFrame),
        resolveTrack(snapshot, PlaybackTrackType::Audio, timelineFrame)
    };
}

} // namespace mini_editor::playback_core
