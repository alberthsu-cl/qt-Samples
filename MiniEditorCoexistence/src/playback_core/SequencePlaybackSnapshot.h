#pragma once

#include "MediaTime.h"
#include "ProjectRuntime.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace mini_editor::playback_core {

enum class PlaybackTrackType { Video, Audio };
enum class PlaybackMediaKind { Video, Audio, Image };
enum class MediaAvailability { Available, Unavailable };

struct PlaybackMediaDescriptor final {
    int mediaAssetId = 0;
    PlaybackMediaKind mediaKind = PlaybackMediaKind::Video;
    std::string immutableSourceLocator;
    MediaAvailability availability = MediaAvailability::Unavailable;
    std::optional<SourceTimestamp> sourceExtent;
};

struct PlaybackClipSettings final {
    int opacityPercent = 100;
    int scalePercent = 100;
    int fadeInFrames = 0;
    int fadeOutFrames = 0;
    int effectKind = 0;
    int effectIntensityPercent = 100;
};

struct PlaybackClip final {
    int clipId = 0;
    int mediaAssetId = 0;
    PlaybackTrackType trackType = PlaybackTrackType::Video;
    TimelineFrame startFrame = TimelineFrame::zero();
    FrameCount duration = FrameCount::zero();
    std::optional<SourceTimestamp> sourceIn;
    PlaybackClipSettings settings;
};

struct SequencePlaybackSnapshot final {
    SequenceId sequenceId;
    SequenceRevision revision;
    FrameRate frameRate;
    FrameCount duration;
    std::vector<PlaybackMediaDescriptor> media;
    std::vector<PlaybackClip> videoClips;
    std::vector<PlaybackClip> audioClips;
    bool isVideoTrackMuted = false;
};

using SequencePlaybackSnapshotPtr = std::shared_ptr<const SequencePlaybackSnapshot>;

struct SnapshotBuildError final {
    std::string message;
};

using SnapshotBuildResult = std::variant<SequencePlaybackSnapshotPtr, SnapshotBuildError>;

} // namespace mini_editor::playback_core
