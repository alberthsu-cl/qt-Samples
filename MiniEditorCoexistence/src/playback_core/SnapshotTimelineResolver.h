#pragma once

#include "PlaybackCommand.h"
#include "SequencePlaybackSnapshot.h"

#include <optional>
#include <string>

namespace mini_editor::playback_core {

// One track's contribution to a timeline frame, resolved from an immutable
// snapshot alone (ADR-003 criterion 4). Everything the caller needs to issue
// a decode request is carried by value, so a resolved frame stays valid even
// if the snapshot it came from is later released.
struct ResolvedSnapshotClip final {
    int clipId;
    MediaAssetId mediaAssetId;
    PlaybackMediaKind mediaKind;
    MediaAvailability availability;
    // Copied rather than pointed at: a caller that hands this to a worker
    // thread must not also have to keep the snapshot alive.
    std::string immutableSourceLocator;
    FrameCount clipLocalFrame;
    FrameCount clipDuration;
    // Empty for a still image, which has no source timeline to seek along.
    std::optional<SourceTimestamp> sourceTime;
    PlaybackClipSettings settings;
};

struct ResolvedSnapshotFrame final {
    TimelineFrame timelineFrame;
    std::optional<ResolvedSnapshotClip> video;
    std::optional<ResolvedSnapshotClip> audio;
};

// ADR-003 migration step 3. The legacy TimelinePlaybackResolver reads live
// TimelineModel/MediaLibrary state, so what it returns depends on when it is
// asked; this one is a pure function of a snapshot, which is what lets a
// stale result be recognized as stale instead of silently winning a race.
//
// Deliberately not computed here: the fade ramp. ClipFade is the legacy
// path's fade policy and lives in the application target, so duplicating its
// arithmetic in the core would create two policies that can disagree. The
// clip's settings are passed through instead, and the presentation step
// applies the single shared policy (M5-05).
class SnapshotTimelineResolver final {
public:
    // TimelinePlaybackResolver takes a raw int and has to clamp negatives to
    // zero. There is no counterpart here: TimelineFrame refuses a negative
    // frame number at construction, so the clamp is enforced by the type
    // rather than repeated by every caller (ADR-001).
    static ResolvedSnapshotFrame resolve(const SequencePlaybackSnapshot &snapshot,
                                         TimelineFrame timelineFrame);

    static std::optional<ResolvedSnapshotClip> resolveTrack(
        const SequencePlaybackSnapshot &snapshot, PlaybackTrackType trackType,
        TimelineFrame timelineFrame);
};

} // namespace mini_editor::playback_core
