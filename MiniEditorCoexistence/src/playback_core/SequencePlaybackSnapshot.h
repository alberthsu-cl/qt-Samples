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

// ADR-003/ADR-006 rule 5. An incoming snapshot supersedes what is currently
// installed when it names a different sequence -- a newly introduced runtime
// sequence, since a reload never reuses a SequenceId -- or a strictly newer
// revision of the same one. Pass no installed revision when nothing has been
// installed yet.
//
// PlaybackSession enforces this; adapters use the same predicate to avoid
// sending an install they know will be refused. One function so the two can
// never drift apart.
inline bool snapshotSupersedes(SequenceId installedSequenceId,
                               std::optional<SequenceRevision> installedRevision,
                               const SequencePlaybackSnapshot &incoming)
{
    return installedSequenceId != incoming.sequenceId
        || !installedRevision
        || *installedRevision < incoming.revision;
}

struct SnapshotBuildError final {
    std::string message;
};

using SnapshotBuildResult = std::variant<SequencePlaybackSnapshotPtr, SnapshotBuildError>;

} // namespace mini_editor::playback_core
