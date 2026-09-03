#include "PreviewPresentation.h"

#include <atomic>

namespace mini_editor::playback_core {
namespace {

std::atomic<std::uint64_t> nextPresentationSessionId { 1 };

PresentationTarget deriveTarget(const PlaybackStatus &status)
{
    if (const auto *source = std::get_if<SourcePreviewStatus>(&status.context)) {
        return PresentationTarget{SourcePresentationTarget{source->sourceId, source->sourceTime}};
    }

    const auto &sequence = std::get<SequencePreviewStatus>(status.context);
    return PresentationTarget{
        SequencePresentationTarget{sequence.sequenceId, sequence.revision, sequence.timelineFrame}
    };
}

} // namespace

PresentationSessionId::PresentationSessionId(std::uint64_t value) : value_(value) {}
PresentationSessionId PresentationSessionId::create()
{
    return PresentationSessionId(nextPresentationSessionId.fetch_add(1));
}
std::uint64_t PresentationSessionId::valueForDiagnostics() const { return value_; }
bool operator==(PresentationSessionId left, PresentationSessionId right)
{
    return left.valueForDiagnostics() == right.valueForDiagnostics();
}
bool operator!=(PresentationSessionId left, PresentationSessionId right) { return !(left == right); }

PresentationRequestId::PresentationRequestId(std::uint64_t value) : value_(value) {}
PresentationRequestId PresentationRequestId::initial() { return PresentationRequestId(0); }
std::uint64_t PresentationRequestId::value() const { return value_; }
PresentationRequestId PresentationRequestId::next() const { return PresentationRequestId(value_ + 1); }
bool operator==(PresentationRequestId left, PresentationRequestId right)
{
    return left.value() == right.value();
}
bool operator!=(PresentationRequestId left, PresentationRequestId right) { return !(left == right); }

bool operator==(const TransportPresentationIdentity &left, const TransportPresentationIdentity &right)
{
    return left.sessionId == right.sessionId && left.generation == right.generation;
}
bool operator!=(const TransportPresentationIdentity &left, const TransportPresentationIdentity &right)
{
    return !(left == right);
}

bool operator==(const EditingPresentationIdentity &left, const EditingPresentationIdentity &right)
{
    return left.sequenceId == right.sequenceId && left.revision == right.revision;
}
bool operator!=(const EditingPresentationIdentity &left, const EditingPresentationIdentity &right)
{
    return !(left == right);
}

bool operator==(const SourcePresentationTarget &left, const SourcePresentationTarget &right)
{
    return left.mediaAssetId == right.mediaAssetId && left.sourceTimestamp == right.sourceTimestamp;
}
bool operator!=(const SourcePresentationTarget &left, const SourcePresentationTarget &right)
{
    return !(left == right);
}

bool operator==(const SequencePresentationTarget &left, const SequencePresentationTarget &right)
{
    return left.sequenceId == right.sequenceId
        && left.sequenceRevision == right.sequenceRevision
        && left.timelineFrame == right.timelineFrame;
}
bool operator!=(const SequencePresentationTarget &left, const SequencePresentationTarget &right)
{
    return !(left == right);
}

bool operator==(const FramePresentationRequest &left, const FramePresentationRequest &right)
{
    return left.presentationSessionId == right.presentationSessionId
        && left.requestId == right.requestId
        && left.authority == right.authority
        && left.target == right.target;
}
bool operator!=(const FramePresentationRequest &left, const FramePresentationRequest &right)
{
    return !(left == right);
}

PreviewPresentationCoordinator::PreviewPresentationCoordinator()
    : presentationSessionId_(PresentationSessionId::create())
    , nextRequestId_(PresentationRequestId::initial())
{
}

void PreviewPresentationCoordinator::issueRequest(PresentationAuthority authority,
                                                  PresentationTarget target, bool forceNew)
{
    if (!forceNew && currentRequest_
        && currentRequest_->authority == authority && currentRequest_->target == target) {
        return;
    }

    currentRequest_ = FramePresentationRequest{
        presentationSessionId_, nextRequestId_, std::move(authority), std::move(target)
    };
    nextRequestId_ = nextRequestId_.next();
}

void PreviewPresentationCoordinator::notifyPlaybackStatus(const PlaybackStatus &status,
                                                          bool transportJustRepositioned)
{
    lastPhase_ = status.phase;

    if (status.phase == PlaybackPhase::Failed)
        return; // retain the last accepted frame while the error is presented.

    const bool transportActive = status.phase == PlaybackPhase::Playing
        || status.phase == PlaybackPhase::Seeking
        || status.phase == PlaybackPhase::Prerolling;

    if (transportActive) {
        editingOverrideActive_ = false;
    } else if (transportJustRepositioned) {
        editingOverrideActive_ = false;
    }

    if (transportActive || !editingOverrideActive_) {
        const PresentationAuthority authority = PresentationAuthority{
            TransportPresentationIdentity{status.sessionId, status.generation}
        };
        issueRequest(authority, deriveTarget(status), transportJustRepositioned);
    }
}

void PreviewPresentationCoordinator::notifyEditingSelection(EditingPresentationIdentity identity,
                                                            PresentationTarget target)
{
    if (lastPhase_ != PlaybackPhase::Stopped && lastPhase_ != PlaybackPhase::Paused)
        return; // a selection cannot override active transport (ADR-003).

    editingOverrideActive_ = true;
    issueRequest(PresentationAuthority{std::move(identity)}, std::move(target), /*forceNew=*/true);
}

void PreviewPresentationCoordinator::clear()
{
    currentRequest_.reset();
    editingOverrideActive_ = false;
    nextRequestId_ = nextRequestId_.next();
}

void PreviewPresentationCoordinator::shutdown()
{
    clear();
}

std::optional<FramePresentationRequest> PreviewPresentationCoordinator::currentRequest() const
{
    return currentRequest_;
}

bool PreviewPresentationCoordinator::isCurrentRequest(PresentationSessionId presentationSessionId,
                                                      PresentationRequestId requestId,
                                                      const PresentationAuthority &authority) const
{
    return currentRequest_
        && currentRequest_->presentationSessionId == presentationSessionId
        && currentRequest_->requestId == requestId
        && currentRequest_->authority == authority;
}

} // namespace mini_editor::playback_core
