#include "SequencePlaybackSnapshotBuilder.h"

#include "ClipFade.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>

namespace {

using namespace mini_editor::playback_core;

SnapshotBuildError invalid(const char *message)
{
    return { message };
}

PlaybackMediaKind mediaKind(MediaKind kind)
{
    switch (kind) {
    case MediaKind::Video: return PlaybackMediaKind::Video;
    case MediaKind::Audio: return PlaybackMediaKind::Audio;
    case MediaKind::Image: return PlaybackMediaKind::Image;
    }
    return PlaybackMediaKind::Video;
}

PlaybackTrackType trackType(TimelineTrackType track)
{
    return track == TimelineTrackType::Audio
        ? PlaybackTrackType::Audio : PlaybackTrackType::Video;
}

SourceTimestamp legacySourceTime(int frame)
{
    return SourceTimestamp::fromMicroseconds(
        sequenceTimeAtFrameStart(TimelineFrame::fromFrameNumber(frame),
                                 FrameRate(30, 1)).microsecondsForAdapter());
}

} // namespace

SnapshotBuildResult SequencePlaybackSnapshotBuilder::build(
    const EditorProject &project, const ProjectRuntime &runtime)
{
    if (!runtime.activeSequenceId() || runtime.sequences().size() != 1)
        return invalid("The project has no active sequence to snapshot.");

    const TimelineSequence &sequence = runtime.sequences().front();
    auto snapshot = std::make_shared<SequencePlaybackSnapshot>(
        SequencePlaybackSnapshot { sequence.id, sequence.revision,
                                   sequence.frameRate, FrameCount::zero(), {}, {}, {},
                                   project.timelineAudioMix.isVideoTrackMuted });
    std::unordered_map<int, const LibraryMediaAsset *> assets;
    for (const LibraryMediaAsset &asset : project.mediaAssets) {
        if (asset.id <= 0 || !assets.emplace(asset.id, &asset).second)
            return invalid("Project media asset identities must be unique and positive.");
    }

    for (const TimelineClip &clip : project.timelineItems) {
        const auto assetIt = assets.find(clip.mediaAssetId);
        if (clip.id <= 0 || assetIt == assets.end() || clip.state.startFrame < 0
            || clip.state.durationFrames <= 0) {
            return invalid("A timeline clip has an invalid identity or media reference.");
        }
        const LibraryMediaAsset &asset = *assetIt->second;
        const bool isStill = asset.kind == MediaKind::Image;
        if ((isStill && clip.state.sourceInFrame != 0)
            || (!isStill && (clip.state.sourceInFrame < 0
                || clip.state.sourceInFrame + clip.state.durationFrames
                    > asset.timelineDurationFrames))) {
            return invalid("A timeline clip has an invalid source range.");
        }
        if (clip.settings.fadeInFrames < 0 || clip.settings.fadeOutFrames < 0
            || clip.settings.fadeInFrames + clip.settings.fadeOutFrames
                > clip.state.durationFrames) {
            return invalid("A timeline clip has invalid fade settings.");
        }

        PlaybackClip playbackClip {
            clip.id, clip.mediaAssetId, trackType(clip.trackType),
            TimelineFrame::fromFrameNumber(clip.state.startFrame),
            FrameCount::fromFrames(clip.state.durationFrames),
            isStill ? std::nullopt
                    : std::optional<SourceTimestamp>(legacySourceTime(clip.state.sourceInFrame)),
            { clip.settings.opacityPercent, clip.settings.scalePercent,
              clip.settings.fadeInFrames, clip.settings.fadeOutFrames,
              static_cast<int>(clip.settings.effect), clip.settings.effectIntensityPercent }
        };
        std::vector<PlaybackClip> &track = clip.trackType == TimelineTrackType::Audio
            ? snapshot->audioClips : snapshot->videoClips;
        track.push_back(std::move(playbackClip));
    }

    auto sortByStart = [](const PlaybackClip &left, const PlaybackClip &right) {
        return left.startFrame < right.startFrame
            || (left.startFrame == right.startFrame && left.clipId < right.clipId);
    };
    std::sort(snapshot->videoClips.begin(), snapshot->videoClips.end(), sortByStart);
    std::sort(snapshot->audioClips.begin(), snapshot->audioClips.end(), sortByStart);

    int greatestEnd = 0;
    auto validateTrack = [&greatestEnd](const std::vector<PlaybackClip> &track) -> bool {
        int previousEnd = 0;
        for (const PlaybackClip &clip : track) {
            const int start = static_cast<int>(clip.startFrame.frameNumber());
            const int end = start + static_cast<int>(clip.duration.frames());
            if (start < previousEnd)
                return false;
            previousEnd = end;
            greatestEnd = std::max(greatestEnd, end);
        }
        return true;
    };
    if (!validateTrack(snapshot->videoClips) || !validateTrack(snapshot->audioClips))
        return invalid("Timeline clips overlap on one playback track.");

    std::unordered_map<int, bool> referencedAssetIds;
    for (const PlaybackClip &clip : snapshot->videoClips)
        referencedAssetIds[clip.mediaAssetId] = true;
    for (const PlaybackClip &clip : snapshot->audioClips)
        referencedAssetIds[clip.mediaAssetId] = true;
    for (const auto &[assetId, ignored] : referencedAssetIds) {
        (void)ignored;
        const LibraryMediaAsset &asset = *assets.at(assetId);
        const bool isStill = asset.kind == MediaKind::Image;
        snapshot->media.push_back({ asset.id, mediaKind(asset.kind),
                                    asset.filePath.generic_string(),
                                    std::filesystem::exists(asset.filePath)
                                        ? MediaAvailability::Available
                                        : MediaAvailability::Unavailable,
                                    isStill ? std::nullopt
                                            : std::optional<SourceTimestamp>(
                                                legacySourceTime(asset.timelineDurationFrames)) });
    }
    std::sort(snapshot->media.begin(), snapshot->media.end(),
              [](const PlaybackMediaDescriptor &left, const PlaybackMediaDescriptor &right) {
                  return left.mediaAssetId < right.mediaAssetId;
              });
    snapshot->duration = FrameCount::fromFrames(greatestEnd);
    return std::const_pointer_cast<const SequencePlaybackSnapshot>(snapshot);
}
