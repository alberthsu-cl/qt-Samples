#pragma once

#include "PlaybackClock.h"
#include "PreviewPresentation.h"
#include "SequencePlaybackSnapshot.h"
#include "SnapshotTimelineResolver.h"
#include "VideoWorkScheduler.h"

#include <optional>
#include <string>

namespace mini_editor::playback_core {

// What an adapter's continuous media player has to be pointed at so the
// viewport matches where the engine's transport actually is.
struct PreviewSourceChange final {
    int clipId;
    MediaAssetId mediaAssetId;
    PlaybackMediaKind mediaKind;
    std::string immutableSourceLocator;
    // Empty for a still image: there is no source timeline to seek along, and
    // handing a still to a continuous player is how you get a decoder error
    // instead of a picture.
    std::optional<SourceTimestamp> sourceTime;
};

struct PreviewDriveOutcome final {
    // The playhead moved onto a different V1 clip. Point the player here and,
    // when a source time is given, seek to it.
    std::optional<PreviewSourceChange> openClip;

    // The playhead is over a gap, past the end, or over media the snapshot
    // marked unavailable. Blank the viewport and ask no decoder for anything.
    bool showNothing = false;

    // The A1 lane, resolved independently of V1 (M5-06, decision C). The two
    // tracks have their own clip boundaries, so re-opening one must never
    // interrupt the other -- which is the whole reason the adapter owns two
    // players rather than one.
    std::optional<PreviewSourceChange> openAudioClip;

    // No A1 clip under the playhead, or its media is unavailable. Silence the
    // lane; do not tear it down.
    bool silenceAudio = false;

    // The A1 clip's fade ramp at this frame, from the one shared fade policy.
    // Meaningless while silenceAudio is set.
    int audioLevelPercent = 100;

    // The snapshot's mix state, applied to the *video* player's own audio.
    bool isVideoTrackMuted = false;

    // The playhead jumped rather than advanced, and this lane's clip did not
    // change -- so its player is still running from where it was and must be
    // moved. Opening a clip already seeks it, so these are set only when no
    // open was issued for that lane, and never both with the corresponding
    // openClip/openAudioClip.
    //
    // Without this, a seek during playback repositioned only whichever lane
    // happened to cross a clip boundary. The other kept playing from the old
    // position, and the two were audible together for the rest of the clip.
    std::optional<SourceTimestamp> repositionVideoTo;
    std::optional<SourceTimestamp> repositionAudioTo;
};

// The glue Milestone 4 built the pieces for but never assembled: turns one
// PlaybackStatus plus the installed snapshot into a presentation identity
// (PreviewPresentationCoordinator), a bounded latest-wins decode request
// (VideoWorkScheduler), and whatever the adapter's player must be told.
//
// It lives in the core, not in TimelineEngineRouter, because every rule here
// -- which clip is under the playhead, when that is a source switch, when a
// gap must blank the viewport, when a decode may be requested at all -- is
// decidable from values alone and so can be tested without Qt, a media file,
// or a window.
//
// This driver never decides transport. It only reads the status
// PlaybackSession published and works out what the viewport should show.
class SequencePreviewDriver final {
public:
    SequencePreviewDriver(PreviewPresentationCoordinator &coordinator,
                          VideoWorkScheduler &scheduler, const IPlaybackClock &clock);

    // A null snapshot is ignored. Installing a snapshot forgets which clip is
    // open, so the next status re-opens against the new content rather than
    // trusting a clip id from the timeline that was just replaced.
    void installSnapshot(SequencePlaybackSnapshotPtr snapshot);

    // Call for every PlaybackStatus the engine publishes, and again whenever
    // the adapter samples the clock while Playing -- without that sampling
    // nothing notices the playhead crossing a clip boundary, because a
    // free-running transport publishes no status of its own.
    //
    // `transportJustRepositioned` has the meaning
    // PreviewPresentationCoordinator gives it: true for a status produced by
    // one of ADR-003's override-clearing commands, false for a plain refresh.
    // It is also what tells this driver the playhead may have jumped rather
    // than advanced, so a caller that passes it loosely will make continuous
    // players re-seek for no reason -- and one that never passes it will
    // leave them behind after a seek.
    PreviewDriveOutcome notifyPlaybackStatus(const PlaybackStatus &status,
                                             bool transportJustRepositioned);

    // The clips the adapter was last told to open, if any.
    std::optional<int> openClipId() const;
    std::optional<int> openAudioClipId() const;

private:
    // A1 has no presentation identity and no bounded latest-wins policy of
    // its own: ADR-003's scheduler is a video policy, and ADR-004 defers
    // audio buffering. Resolving which clip is under the playhead is
    // identical for both tracks, though, so it is shared.
    void driveAudioLane(const ResolvedSnapshotFrame &resolved, bool transportJustRepositioned,
                        PreviewDriveOutcome &outcome);

    PreviewPresentationCoordinator &coordinator_;
    VideoWorkScheduler &scheduler_;
    const IPlaybackClock &clock_;
    SequencePlaybackSnapshotPtr snapshot_;
    std::optional<int> openClipId_;
    std::optional<int> openAudioClipId_;
};

} // namespace mini_editor::playback_core
