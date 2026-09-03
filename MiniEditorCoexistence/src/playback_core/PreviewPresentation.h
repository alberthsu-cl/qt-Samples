#pragma once

#include "PlaybackCommand.h"

#include <cstdint>
#include <optional>
#include <variant>

namespace mini_editor::playback_core {

// Identifies one PreviewPresentationCoordinator's lifetime. A new
// presentation session is created whenever the coordinator (or its project
// runtime) is recreated (ADR-003).
class PresentationSessionId final {
public:
    static PresentationSessionId create();

    std::uint64_t valueForDiagnostics() const;

private:
    explicit PresentationSessionId(std::uint64_t value);

    std::uint64_t value_;
};

bool operator==(PresentationSessionId left, PresentationSessionId right);
bool operator!=(PresentationSessionId left, PresentationSessionId right);

// Scoped to one PresentationSessionId; increments whenever the desired
// visual target changes (ADR-003).
class PresentationRequestId final {
public:
    static PresentationRequestId initial();

    std::uint64_t value() const;
    PresentationRequestId next() const;

private:
    explicit PresentationRequestId(std::uint64_t value);

    std::uint64_t value_;
};

bool operator==(PresentationRequestId left, PresentationRequestId right);
bool operator!=(PresentationRequestId left, PresentationRequestId right);

struct TransportPresentationIdentity final {
    PlaybackSessionId sessionId;
    PlaybackGeneration generation;
};

struct EditingPresentationIdentity final {
    SequenceId sequenceId;
    SequenceRevision revision;
};

bool operator==(const TransportPresentationIdentity &left, const TransportPresentationIdentity &right);
bool operator!=(const TransportPresentationIdentity &left, const TransportPresentationIdentity &right);
bool operator==(const EditingPresentationIdentity &left, const EditingPresentationIdentity &right);
bool operator!=(const EditingPresentationIdentity &left, const EditingPresentationIdentity &right);

using PresentationAuthority =
    std::variant<TransportPresentationIdentity, EditingPresentationIdentity>;

struct SourcePresentationTarget final {
    MediaAssetId mediaAssetId;
    SourceTimestamp sourceTimestamp;
};

struct SequencePresentationTarget final {
    SequenceId sequenceId;
    SequenceRevision sequenceRevision;
    TimelineFrame timelineFrame;
};

bool operator==(const SourcePresentationTarget &left, const SourcePresentationTarget &right);
bool operator!=(const SourcePresentationTarget &left, const SourcePresentationTarget &right);
bool operator==(const SequencePresentationTarget &left, const SequencePresentationTarget &right);
bool operator!=(const SequencePresentationTarget &left, const SequencePresentationTarget &right);

// A domain-safe source timestamp or sequence frame. A sequence target
// carries only TimelineFrame; clip identity is resolved from the referenced
// (SequenceId, SequenceRevision) snapshot elsewhere, so parallel position
// fields cannot disagree (ADR-003).
using PresentationTarget = std::variant<SourcePresentationTarget, SequencePresentationTarget>;

struct FramePresentationRequest final {
    PresentationSessionId presentationSessionId;
    PresentationRequestId requestId;
    PresentationAuthority authority;
    PresentationTarget target;
};

bool operator==(const FramePresentationRequest &left, const FramePresentationRequest &right);
bool operator!=(const FramePresentationRequest &left, const FramePresentationRequest &right);

// ADR-003: owns the desired visual target for one preview viewport,
// separate from PlaybackSession's transport authority. This milestone
// (M4-01) only manages request identity -- no real frame candidate exists
// yet (that starts in M4-03/M4-04), so there is no accept/reject-a-frame
// method here; isCurrentRequest() is what a future frame consumer will call.
class PreviewPresentationCoordinator final {
public:
    PreviewPresentationCoordinator();

    // Called whenever a fresh PlaybackStatus is available. `transportJustRepositioned`
    // is true when the command that produced this status was one of the
    // ADR-003-listed override-clearing commands (Play, Seek, Stop,
    // OpenSource, InstallSnapshot) -- the caller already knows which command
    // it just submitted, so that classification is not re-derived here from
    // PlaybackStatus alone (which does not carry the command's kind, only
    // its opaque id). Pass false for a plain status refresh (e.g. sampling
    // the clock while Playing, or a Pause/SetRate) that must not clear an
    // active editing-preview override or force a new request id.
    //
    // While Playing/Seeking/Prerolling, this always becomes the desired
    // transport presentation and clears any editing override. While
    // Stopped/Paused, it becomes the desired presentation only if no
    // editing override is active. While Failed, no new request is issued --
    // the last accepted frame is retained.
    void notifyPlaybackStatus(const PlaybackStatus &status, bool transportJustRepositioned);

    // Explicit timeline-clip/source-asset selection. Only takes effect while
    // the most recently notified phase was Stopped or Paused -- a selection
    // cannot override active transport (ADR-003). Always issues a new
    // request id, matching "clip selection" in ADR-003's bump list.
    void notifyEditingSelection(EditingPresentationIdentity identity, PresentationTarget target);

    // Removes the current desired request entirely (e.g. viewport hidden).
    // Absence of a later request must not implicitly clear one -- only this
    // does.
    void clear();

    // Ends this coordinator's presentation session. A later
    // PreviewPresentationCoordinator instance gets a fresh
    // PresentationSessionId (ADR-003: "a new presentation session is
    // created whenever the coordinator... is recreated").
    void shutdown();

    std::optional<FramePresentationRequest> currentRequest() const;

    // ADR-003's acceptance rule for a frame candidate: current presentation
    // session, newest desired request id, matching authority. A future
    // frame consumer (M4-03/M4-04) calls this before accepting a candidate.
    bool isCurrentRequest(PresentationSessionId presentationSessionId,
                          PresentationRequestId requestId,
                          const PresentationAuthority &authority) const;

private:
    void issueRequest(PresentationAuthority authority, PresentationTarget target, bool forceNew);

    PresentationSessionId presentationSessionId_;
    PresentationRequestId nextRequestId_;
    std::optional<FramePresentationRequest> currentRequest_;
    PlaybackPhase lastPhase_ = PlaybackPhase::Stopped;
    bool editingOverrideActive_ = false;
};

} // namespace mini_editor::playback_core
