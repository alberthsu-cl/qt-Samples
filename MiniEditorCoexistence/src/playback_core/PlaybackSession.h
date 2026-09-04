#pragma once

#include "PlaybackClock.h"
#include "PlaybackCommand.h"

#include <optional>

namespace mini_editor::playback_core {

// ADR-002's sole playback-state authority, applied synchronously.
//
// This milestone has no engine thread, decoder, or notification queue
// (ADR-005 -- Milestone 4): applyCommand() runs the whole command-policy
// table to completion in one call. With no real decode/preroll work to wait
// for, Seeking/Prerolling resolve to their destination phase within that same
// call rather than being observed as a separate step -- the same "completes
// without a decoder" case ADR-002/ADR-004 already describe for a still image
// or gap. A future engine thread makes those phases observable between
// commands without changing this class's transition rules.
//
// A session is constructed for exactly one PlaybackSource *kind* and keeps
// that kind for its lifetime: OpenSource switches which asset a
// SourceAssetPreview session shows; InstallSnapshot switches which content a
// SequencePreview session shows. A command whose payload names the other
// kind is rejected with SourceKindMismatch rather than silently accepted.
//
// Which sequence a SequencePreview session shows is not fixed at
// construction. A project reload mints a new SequenceId (ADR-006 rule 2), so
// InstallSnapshot retargets the session onto the incoming sequence, keeping
// the PlaybackSessionId and advancing the generation -- exactly what ADR-006
// specifies for a reload while the engine session continues to run. For a
// sequence already installed, a revision that is not strictly newer is
// rejected instead, so a duplicate or out-of-order install cannot roll
// playback content backward.
class PlaybackSession final {
public:
    PlaybackSession(PlaybackSource initialSource, const IPlaybackClock &clock);

    // Applies one command to completion. Returns the rejection if the
    // command was not accepted; returns nullopt otherwise, and the
    // acceptance is reflected in status() (including
    // PlaybackStatus::lastAppliedCommandId).
    std::optional<PlaybackCommandRejected> applyCommand(const PlaybackCommand &command,
                                                        PlaybackCommandId commandId);

    // Built fresh on every call: a Playing SequencePreview's timelineFrame is
    // resolved from the clock and anchor, never cached (ADR-004).
    PlaybackStatus status() const;

    // True when sessionId/generation still match this session's current
    // state -- the identity check ADR-002 requires before any asynchronous
    // observation is allowed to have an effect.
    bool isCurrent(PlaybackSessionId sessionId, PlaybackGeneration generation) const;

    // The one observation this milestone wires up: a validated failure
    // enters Failed and publishes its error (ADR-002). Real decoder/media
    // failure delivery is Milestone 4; this is the seam it will call. A
    // stale sessionId/generation is discarded, exactly like any other
    // observation (ADR-002 criterion 8, generalized further in M3-03).
    void reportFailure(PlaybackSessionId sessionId, PlaybackGeneration generation,
                       PlaybackError error);

    // ADR-002: "a QMediaPlayer::positionChanged callback may report a
    // candidate SourceTimestamp; after identity validation, the session may
    // adopt it as the authoritative source position." Only meaningful for a
    // SourceAssetPreview session; a stale sessionId/generation, or a report
    // arriving for a SequencePreview session, is discarded like any other
    // observation. Adopting the same position twice is idempotent -- it
    // does not advance statusSeq if the position does not actually change.
    void reportSourcePosition(PlaybackSessionId sessionId, PlaybackGeneration generation,
                              SourceTimestamp position);

    // ADR-002's natural completion -- the one transition no command causes.
    // "Sequence preview with ReturnToStart finishes in Stopped at timeline
    // frame zero": that end is reached by the clock, not by anything the user
    // did, so somebody has to look. This is that look, and it is an
    // observation like any other: it applies the transition, it does not
    // decide it.
    //
    // Called automatically at the start of every applyCommand(), so a
    // command arriving after the end never acts on a past-the-end position.
    // Callers that let the clock run freely (PlaybackEngine, and any test
    // driving a session directly) must also call it as time passes, or the
    // position keeps growing past the sequence duration until the next
    // command lands.
    void observeClock();

private:
    bool isSequenceMode() const;
    bool hasReachedSequenceEnd() const;
    PlaybackContext buildContext() const;
    SequenceTime clampToSnapshotDuration(SequenceTime time) const;
    SourceTimestamp clampSourceTime(SourceTimestamp time) const;
    void resetToContextStart();
    void bumpStatusSeq(PlaybackCommandId commandId);

    std::optional<PlaybackCommandRejected> applyOpenSource(const OpenSource &command,
                                                            PlaybackCommandId commandId);
    std::optional<PlaybackCommandRejected> applyInstallSnapshot(const InstallSnapshot &command,
                                                                PlaybackCommandId commandId);
    std::optional<PlaybackCommandRejected> applyPlay(PlaybackCommandId commandId);
    std::optional<PlaybackCommandRejected> applyPause(PlaybackCommandId commandId);
    std::optional<PlaybackCommandRejected> applyStop(PlaybackCommandId commandId);
    std::optional<PlaybackCommandRejected> applySeek(const Seek &command,
                                                      PlaybackCommandId commandId);
    std::optional<PlaybackCommandRejected> applySetRate(const SetRate &command,
                                                         PlaybackCommandId commandId);
    std::optional<PlaybackCommandRejected> applyShutdown(PlaybackCommandId commandId);

    const IPlaybackClock &clock_;

    PlaybackSessionId sessionId_;
    PlaybackGeneration generation_;
    StatusSequenceNumber statusSeq_;
    PlaybackPhase phase_ = PlaybackPhase::Stopped;
    PlaybackSource source_;
    int ratePercent_ = 100;
    std::optional<PlaybackError> error_;
    std::optional<PlaybackCommandId> lastAppliedCommandId_;
    bool acceptingCommands_ = true;

    // SequencePreview-mode state.
    SequencePlaybackSnapshotPtr snapshot_;
    PlaybackAnchor anchor_;
    FrameRate sequenceFrameRateBeforeSnapshot_ = FrameRate(30, 1);

    // SourceAssetPreview-mode state.
    SourceTimestamp sourceTime_ = SourceTimestamp::fromMicroseconds(0);
    SourceTimestamp sourceEndTime_ = SourceTimestamp::fromMicroseconds(0);
    SourceCompletionPolicy completionPolicy_ = SourceCompletionPolicy::HoldLastFrame;
};

} // namespace mini_editor::playback_core
