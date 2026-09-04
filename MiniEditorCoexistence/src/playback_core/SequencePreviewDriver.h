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
    // The playhead moved onto a different clip. Point the player here and,
    // when a source time is given, seek to it.
    std::optional<PreviewSourceChange> openClip;

    // The playhead is over a gap, past the end, or over media the snapshot
    // marked unavailable. Blank the viewport and ask no decoder for anything.
    bool showNothing = false;
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
    PreviewDriveOutcome notifyPlaybackStatus(const PlaybackStatus &status,
                                             bool transportJustRepositioned);

    // The clip the adapter was last told to open, if any.
    std::optional<int> openClipId() const;

private:
    PreviewPresentationCoordinator &coordinator_;
    VideoWorkScheduler &scheduler_;
    const IPlaybackClock &clock_;
    SequencePlaybackSnapshotPtr snapshot_;
    std::optional<int> openClipId_;
};

} // namespace mini_editor::playback_core
