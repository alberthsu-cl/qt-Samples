#include "PlaybackSession.h"

#include <utility>

namespace mini_editor::playback_core {

PlaybackSession::PlaybackSession(PlaybackSource initialSource, const IPlaybackClock &clock)
    : clock_(clock)
    , sessionId_(PlaybackSessionId::create())
    , generation_(PlaybackGeneration::initial())
    , statusSeq_(StatusSequenceNumber::initial())
    , source_(std::move(initialSource))
    , anchor_{clock.now(), SequenceTime::zero(), ratePercent_}
{
}

bool PlaybackSession::isSequenceMode() const
{
    return std::holds_alternative<SequencePreview>(source_);
}

PlaybackContext PlaybackSession::buildContext() const
{
    if (!isSequenceMode()) {
        return PlaybackContext{SourcePreviewStatus{
            std::get<SourceAssetPreview>(source_).assetId,
            sourceTime_,
            sourceEndTime_,
            completionPolicy_
        }};
    }

    const FrameRate frameRate = snapshot_ ? snapshot_->frameRate : sequenceFrameRateBeforeSnapshot_;
    const FrameCount duration = snapshot_ ? snapshot_->duration : FrameCount::zero();
    const SequenceRevision revision = snapshot_ ? snapshot_->revision : SequenceRevision::initial();
    const SequenceTime position = (phase_ == PlaybackPhase::Playing)
        ? resolveSequenceTime(anchor_, clock_)
        : anchor_.sequenceTime;

    return PlaybackContext{SequencePreviewStatus{
        std::get<SequencePreview>(source_).sequenceId,
        revision,
        frameAtSequenceTime(position, frameRate),
        duration,
        frameRate
    }};
}

SequenceTime PlaybackSession::clampToSnapshotDuration(SequenceTime time) const
{
    if (!snapshot_)
        return SequenceTime::zero();

    const SequenceTime start = SequenceTime::zero();
    if (time < start)
        return start;

    const TimelineFrame endFrame = TimelineFrame::zero() + snapshot_->duration;
    const SequenceTime end = sequenceTimeAtFrameStart(endFrame, snapshot_->frameRate);
    if (time > end)
        return end;

    return time;
}

SourceTimestamp PlaybackSession::clampSourceTime(SourceTimestamp time) const
{
    if (time < sourceTimeZero())
        return sourceTimeZero();
    if (time > sourceEndTime_)
        return sourceEndTime_;
    return time;
}

void PlaybackSession::resetToContextStart()
{
    if (isSequenceMode())
        anchor_ = PlaybackAnchor{clock_.now(), SequenceTime::zero(), ratePercent_};
    else
        sourceTime_ = sourceTimeZero();
}

bool PlaybackSession::hasReachedSequenceEnd() const
{
    if (!isSequenceMode() || phase_ != PlaybackPhase::Playing || !snapshot_)
        return false;

    // The end is the first sequence time not covered by the last frame. A
    // sequence with no content has no end to reach; Play on an empty
    // timeline stays Playing at frame zero, exactly as the legacy path does.
    if (snapshot_->duration <= FrameCount::zero())
        return false;

    const TimelineFrame endFrame = TimelineFrame::zero() + snapshot_->duration;
    return resolveSequenceTime(anchor_, clock_)
        >= sequenceTimeAtFrameStart(endFrame, snapshot_->frameRate);
}

void PlaybackSession::observeClock()
{
    if (!hasReachedSequenceEnd())
        return;

    // ADR-002: natural completion increments the generation and invalidates
    // pending media work, then finishes in Stopped at timeline frame zero.
    generation_ = generation_.next();
    resetToContextStart();
    phase_ = PlaybackPhase::Stopped;
    statusSeq_ = statusSeq_.next();
    // lastAppliedCommandId_ is deliberately left alone: no command was
    // applied, and claiming one was would misreport which request the UI is
    // seeing the result of.
}

void PlaybackSession::bumpStatusSeq(PlaybackCommandId commandId)
{
    statusSeq_ = statusSeq_.next();
    lastAppliedCommandId_ = commandId;
}

PlaybackStatus PlaybackSession::status() const
{
    return PlaybackStatus{
        sessionId_,
        generation_,
        statusSeq_,
        buildContext(),
        phase_,
        ratePercent_,
        error_,
        lastAppliedCommandId_
    };
}

bool PlaybackSession::isCurrent(PlaybackSessionId sessionId, PlaybackGeneration generation) const
{
    return sessionId_ == sessionId && generation_ == generation;
}

void PlaybackSession::reportFailure(PlaybackSessionId sessionId, PlaybackGeneration generation,
                                    PlaybackError error)
{
    if (!isCurrent(sessionId, generation))
        return;
    if (phase_ == PlaybackPhase::Failed)
        return; // already Failed for this identity: idempotent, no new transition

    phase_ = PlaybackPhase::Failed;
    error_ = std::move(error);
    statusSeq_ = statusSeq_.next();
}

void PlaybackSession::reportSourcePosition(PlaybackSessionId sessionId, PlaybackGeneration generation,
                                           SourceTimestamp position)
{
    if (!isCurrent(sessionId, generation) || isSequenceMode())
        return;

    const SourceTimestamp clamped = clampSourceTime(position);
    if (clamped == sourceTime_)
        return;

    sourceTime_ = clamped;
    statusSeq_ = statusSeq_.next();
}

std::optional<PlaybackCommandRejected> PlaybackSession::applyCommand(const PlaybackCommand &command,
                                                                      PlaybackCommandId commandId)
{
    if (!acceptingCommands_)
        return PlaybackCommandRejected{commandId, PlaybackRejectReason::QueueClosed};

    // A command that arrives after the sequence finished must act on the
    // completed state, not on a position that ran past the end while nobody
    // was looking. Play after completion therefore starts from frame zero.
    observeClock();

    return std::visit([this, commandId](const auto &cmd) -> std::optional<PlaybackCommandRejected> {
        using T = std::decay_t<decltype(cmd)>;
        if constexpr (std::is_same_v<T, OpenSource>) {
            return applyOpenSource(cmd, commandId);
        } else if constexpr (std::is_same_v<T, InstallSnapshot>) {
            return applyInstallSnapshot(cmd, commandId);
        } else if constexpr (std::is_same_v<T, Play>) {
            return applyPlay(commandId);
        } else if constexpr (std::is_same_v<T, Pause>) {
            return applyPause(commandId);
        } else if constexpr (std::is_same_v<T, Stop>) {
            return applyStop(commandId);
        } else if constexpr (std::is_same_v<T, Seek>) {
            return applySeek(cmd, commandId);
        } else if constexpr (std::is_same_v<T, SetRate>) {
            return applySetRate(cmd, commandId);
        } else {
            static_assert(std::is_same_v<T, Shutdown>);
            return applyShutdown(commandId);
        }
    }, command);
}

std::optional<PlaybackCommandRejected> PlaybackSession::applyOpenSource(const OpenSource &command,
                                                                        PlaybackCommandId commandId)
{
    if (isSequenceMode())
        return PlaybackCommandRejected{commandId, PlaybackRejectReason::SourceKindMismatch};

    generation_ = generation_.next();
    source_ = PlaybackSource{SourceAssetPreview{command.assetId}};
    sourceTime_ = sourceTimeZero();
    sourceEndTime_ = command.sourceEndTime;
    completionPolicy_ = command.completionPolicy;
    error_.reset();
    phase_ = PlaybackPhase::Stopped;

    bumpStatusSeq(commandId);
    return std::nullopt;
}

std::optional<PlaybackCommandRejected> PlaybackSession::applyInstallSnapshot(const InstallSnapshot &command,
                                                                             PlaybackCommandId commandId)
{
    if (!isSequenceMode())
        return PlaybackCommandRejected{commandId, PlaybackRejectReason::SourceKindMismatch};

    // Which sequence this session is showing right now. Before the first
    // install that is the sequence it was constructed for; afterwards it is
    // whatever snapshot is installed, and the two are kept equal below.
    const SequenceId currentSequenceId = snapshot_
        ? snapshot_->sequenceId : std::get<SequencePreview>(source_).sequenceId;
    const bool retargetsAnotherSequence = currentSequenceId != command.snapshot->sequenceId;

    if (!snapshotSupersedes(currentSequenceId,
                            snapshot_ ? std::optional<SequenceRevision>(snapshot_->revision)
                                      : std::nullopt,
                            *command.snapshot)) {
        return PlaybackCommandRejected{commandId, PlaybackRejectReason::StaleSequenceRevision};
    }

    const bool wasFailed = phase_ == PlaybackPhase::Failed;
    generation_ = generation_.next();
    snapshot_ = command.snapshot;

    if (retargetsAnotherSequence) {
        // A different sequence is different content: a position measured
        // against the old timeline means nothing on the new one, so the
        // transport starts over rather than being clamped into it. ADR-006:
        // the session keeps its PlaybackSessionId across a project reload and
        // advances only its generation, which is what invalidates every piece
        // of work still in flight for the sequence being left behind.
        source_ = PlaybackSource{SequencePreview{command.snapshot->sequenceId}};
        error_.reset();
        phase_ = PlaybackPhase::Stopped;
        anchor_ = PlaybackAnchor{clock_.now(), SequenceTime::zero(), ratePercent_};
    } else if (wasFailed) {
        error_.reset();
        anchor_ = PlaybackAnchor{clock_.now(), SequenceTime::zero(), ratePercent_};
        phase_ = PlaybackPhase::Stopped;
    } else {
        const SequenceTime current = (phase_ == PlaybackPhase::Playing)
            ? resolveSequenceTime(anchor_, clock_)
            : anchor_.sequenceTime;
        anchor_ = PlaybackAnchor{clock_.now(), clampToSnapshotDuration(current), ratePercent_};
    }

    bumpStatusSeq(commandId);
    return std::nullopt;
}

std::optional<PlaybackCommandRejected> PlaybackSession::applyPlay(PlaybackCommandId commandId)
{
    if (phase_ == PlaybackPhase::Failed)
        return PlaybackCommandRejected{commandId, PlaybackRejectReason::InvalidForCurrentPhase};

    if (phase_ != PlaybackPhase::Playing) {
        if (isSequenceMode())
            anchor_ = PlaybackAnchor{clock_.now(), anchor_.sequenceTime, ratePercent_};
        phase_ = PlaybackPhase::Playing;
    }

    bumpStatusSeq(commandId);
    return std::nullopt;
}

std::optional<PlaybackCommandRejected> PlaybackSession::applyPause(PlaybackCommandId commandId)
{
    if (phase_ == PlaybackPhase::Failed)
        return PlaybackCommandRejected{commandId, PlaybackRejectReason::InvalidForCurrentPhase};

    if (phase_ == PlaybackPhase::Playing) {
        if (isSequenceMode())
            anchor_ = PlaybackAnchor{clock_.now(), resolveSequenceTime(anchor_, clock_), ratePercent_};
        phase_ = PlaybackPhase::Paused;
    }

    bumpStatusSeq(commandId);
    return std::nullopt;
}

std::optional<PlaybackCommandRejected> PlaybackSession::applyStop(PlaybackCommandId commandId)
{
    const bool wasFailed = phase_ == PlaybackPhase::Failed;

    if (wasFailed) {
        error_.reset();
        resetToContextStart();
        phase_ = PlaybackPhase::Stopped;
    } else if (phase_ != PlaybackPhase::Stopped) {
        generation_ = generation_.next();
        resetToContextStart();
        phase_ = PlaybackPhase::Stopped;
    }

    bumpStatusSeq(commandId);
    return std::nullopt;
}

std::optional<PlaybackCommandRejected> PlaybackSession::applySeek(const Seek &command,
                                                                  PlaybackCommandId commandId)
{
    if (phase_ == PlaybackPhase::Failed)
        return PlaybackCommandRejected{commandId, PlaybackRejectReason::InvalidForCurrentPhase};

    const bool wasPlaying = phase_ == PlaybackPhase::Playing;

    if (isSequenceMode()) {
        const SequenceTime *target = std::get_if<SequenceTime>(&command.target);
        if (!target)
            return PlaybackCommandRejected{commandId, PlaybackRejectReason::SourceKindMismatch};

        generation_ = generation_.next();
        anchor_ = PlaybackAnchor{clock_.now(), clampToSnapshotDuration(*target), ratePercent_};
    } else {
        const SourceTimestamp *target = std::get_if<SourceTimestamp>(&command.target);
        if (!target)
            return PlaybackCommandRejected{commandId, PlaybackRejectReason::SourceKindMismatch};

        generation_ = generation_.next();
        sourceTime_ = clampSourceTime(*target);
    }

    phase_ = wasPlaying ? PlaybackPhase::Playing : PlaybackPhase::Paused;
    bumpStatusSeq(commandId);
    return std::nullopt;
}

std::optional<PlaybackCommandRejected> PlaybackSession::applySetRate(const SetRate &command,
                                                                     PlaybackCommandId commandId)
{
    if (phase_ == PlaybackPhase::Failed)
        return PlaybackCommandRejected{commandId, PlaybackRejectReason::InvalidForCurrentPhase};

    if (phase_ == PlaybackPhase::Playing && isSequenceMode()) {
        anchor_ = PlaybackAnchor{clock_.now(), resolveSequenceTime(anchor_, clock_),
                                 command.ratePercent};
    }

    ratePercent_ = command.ratePercent;
    bumpStatusSeq(commandId);
    return std::nullopt;
}

std::optional<PlaybackCommandRejected> PlaybackSession::applyShutdown(PlaybackCommandId commandId)
{
    generation_ = generation_.next();
    bumpStatusSeq(commandId);
    acceptingCommands_ = false;
    return std::nullopt;
}

} // namespace mini_editor::playback_core
