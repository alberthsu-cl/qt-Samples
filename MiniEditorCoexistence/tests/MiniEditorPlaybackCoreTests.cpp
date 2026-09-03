#include "PlaybackCoreBoundary.h"
#include "MediaTime.h"
#include "ProjectRuntime.h"
#include "PlaybackClock.h"
#include "PlaybackSession.h"
#include "PlaybackStatusGate.h"
#include "PreviewPresentation.h"

#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {

template <typename Left, typename Right, typename = void>
struct HasAddition : std::false_type {};

template <typename Left, typename Right>
struct HasAddition<Left, Right,
                   std::void_t<decltype(std::declval<Left>()
                                        + std::declval<Right>())>>
    : std::true_type {};

template <typename Left, typename Right, typename = void>
struct HasSubtraction : std::false_type {};

template <typename Left, typename Right>
struct HasSubtraction<Left, Right,
                      std::void_t<decltype(std::declval<Left>()
                                           - std::declval<Right>())>>
    : std::true_type {};

bool require(bool condition, const char *message)
{
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}

// A deterministic test double for IPlaybackClock (ADR-004). Production code
// never sees this type; it exists only so engine tests can control elapsed
// time exactly.
class FakePlaybackClock final : public mini_editor::playback_core::IPlaybackClock {
public:
    explicit FakePlaybackClock(mini_editor::playback_core::MasterClockTime time)
        : time_(time)
    {
    }

    mini_editor::playback_core::MasterClockTime now() const override
    {
        return time_;
    }

    void set(mini_editor::playback_core::MasterClockTime time)
    {
        time_ = time;
    }

private:
    mini_editor::playback_core::MasterClockTime time_;
};

bool throwsInvalidArgumentForInvalidFrameRate()
{
    try {
        mini_editor::playback_core::FrameRate invalid(0, 1);
        (void)invalid;
    } catch (const std::invalid_argument &) {
        return true;
    }

    return false;
}

bool verifyFrameBoundaries(mini_editor::playback_core::FrameRate rate)
{
    using namespace mini_editor::playback_core;

    const TimelineFrame frame = TimelineFrame::fromFrameNumber(123);
    const SequenceTime start = sequenceTimeAtFrameStart(frame, rate);
    const SequenceTime next = sequenceTimeAtNextFrameStart(frame, rate);

    return require(frameAtSequenceTime(start, rate) == frame,
                   "A frame start must resolve to its own frame.")
        && require(frameAtSequenceTime(next, rate)
                       == frame + FrameCount::fromFrames(1),
                   "The next frame start must honor the half-open boundary.")
        && require(start < next, "Every frame interval must have positive duration.");
}

bool verifyTwentyFourHourRange(mini_editor::playback_core::FrameRate rate)
{
    using namespace mini_editor::playback_core;

    const SequenceTime endOfDay = SequenceTime::fromMicroseconds(86'400'000'000);
    const TimelineFrame frame = frameAtSequenceTime(endOfDay, rate);
    const SequenceTime start = sequenceTimeAtFrameStart(frame, rate);
    const SequenceTime next = sequenceTimeAtNextFrameStart(frame, rate);

    return require(start <= endOfDay && endOfDay < next,
                   "A 24-hour value must remain inside its resolved frame interval.");
}

bool verifySourceMapping()
{
    using namespace mini_editor::playback_core;

    const ClipTimeMapping video {
        TimelineFrame::fromFrameNumber(100),
        SourceTimestamp::fromMicroseconds(0),
        FrameRate(30, 1)
    };
    const auto sourceAtStart = sourceTimestampFor(
        TimelineFrame::fromFrameNumber(100), video);
    const auto sourceOneSecondLater = sourceTimestampFor(
        TimelineFrame::fromFrameNumber(130), video);

    const ClipTimeMapping still {
        TimelineFrame::zero(),
        std::nullopt,
        FrameRate(30, 1)
    };

    return require(sourceAtStart && sourceAtStart->microsecondsForAdapter() == 0,
                   "A zero source timestamp must remain valid media time.")
        && require(sourceOneSecondLater
                       && sourceOneSecondLater->microsecondsForAdapter() == 1'000'000,
                   "Source mapping must use a named sequence-to-source conversion.")
        && require(!sourceTimestampFor(TimelineFrame::zero(), still),
                   "Still-image mapping must not invent a source timestamp.");
}

bool verifyRateConversions()
{
    using namespace mini_editor::playback_core;

    return require(sequenceElapsedFor(ClockDuration::fromMicroseconds(1'000'000), 200)
                       == SequenceDuration::fromMicroseconds(2'000'000),
                   "A 200% rate must double sequence elapsed time.")
        && require(clockElapsedFor(SequenceDuration::fromMicroseconds(2'000'000), 200)
                       == ClockDuration::fromMicroseconds(1'000'000),
                   "Clock elapsed time must use the named inverse rate boundary.")
        && require(sequenceElapsedFor(ClockDuration::fromMicroseconds(-1), 50)
                       == SequenceDuration::zero(),
                   "Signed sub-microsecond rate results must truncate toward zero.");
}

bool verifyProjectRuntimeIdentityAndReadiness()
{
    using namespace mini_editor::playback_core;

    ProjectRuntime emptyProject = ProjectRuntime::fromLegacyFlatProject(0);
    ProjectRuntime reloadedProject = ProjectRuntime::fromLegacyFlatProject(0);
    const SequenceId emptySequenceId = emptyProject.sequences().front().id;

    if (!require(emptyProject.readiness() == ProjectReadiness::Empty,
                 "An empty legacy timeline must be an explicit empty project.")
        || !require(emptyProject.sequences().size() == 1
                        && emptyProject.sequences().front().frameRate == FrameRate(30, 1),
                    "A flat legacy project must synthesize one 30 fps sequence.")
        || !require(emptyProject.activeSequenceId()
                        && *emptyProject.activeSequenceId() == emptySequenceId,
                    "The synthesized legacy sequence must be explicitly active.")
        || !require(emptyProject.projectId() != reloadedProject.projectId()
                        && emptySequenceId != reloadedProject.sequences().front().id,
                    "Reloading identical content must create fresh runtime identities.")) {
        return false;
    }

    emptyProject.setLegacySequenceClipCount(2);
    if (!require(emptyProject.readiness() == ProjectReadiness::Ready,
                 "A sequence with timeline clips must become ready.")
        || !require(emptyProject.sequences().front().id == emptySequenceId,
                    "Editing clip count must preserve the active sequence identity.")) {
        return false;
    }

    const ProjectRuntime loadingProject = ProjectRuntime::loading();
    const ProjectRuntime failedProject = ProjectRuntime::failed("Missing project file");
    return require(loadingProject.readiness() == ProjectReadiness::Loading
                       && !loadingProject.error(),
                   "Loading must be distinct from empty and failed states.")
        && require(failedProject.readiness() == ProjectReadiness::Failed
                       && failedProject.error(),
                   "A failed runtime must carry a framework-neutral error.");
}

bool verifyAnchorResolution(mini_editor::playback_core::FrameRate rate)
{
    using namespace mini_editor::playback_core;

    // Anchor: sequence is at 2s when the clock reads 10s, at normal rate.
    const PlaybackAnchor anchor{
        MasterClockTime::fromMicroseconds(10'000'000),
        SequenceTime::fromMicroseconds(2'000'000),
        100
    };

    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(10'000'000));
    if (!require(resolveSequenceTime(anchor, clock)
                     == SequenceTime::fromMicroseconds(2'000'000),
                 "Reading the clock at the anchor instant must not move position.")) {
        return false;
    }

    clock.set(MasterClockTime::fromMicroseconds(13'500'000));
    if (!require(resolveSequenceTime(anchor, clock)
                     == SequenceTime::fromMicroseconds(5'500'000),
                 "Elapsed clock time must advance sequence time at normal rate.")) {
        return false;
    }

    if (!require(resolveTimelineFrame(anchor, clock, rate)
                     == frameAtSequenceTime(SequenceTime::fromMicroseconds(5'500'000), rate),
                 "resolveTimelineFrame must compose the anchor equation with frameAtSequenceTime.")) {
        return false;
    }

    // 200% rate: the same elapsed clock time must move sequence time twice as far.
    const PlaybackAnchor fastAnchor{
        MasterClockTime::fromMicroseconds(0),
        SequenceTime::zero(),
        200
    };
    clock.set(MasterClockTime::fromMicroseconds(1'000'000));
    if (!require(resolveSequenceTime(fastAnchor, clock)
                     == SequenceTime::fromMicroseconds(2'000'000),
                 "A 200% rate anchor must double resolved sequence elapsed time.")) {
        return false;
    }

    // Reading the clock twice without moving it must resolve to the same position.
    return require(resolveSequenceTime(fastAnchor, clock)
                       == resolveSequenceTime(fastAnchor, clock),
                   "Resolving from an unchanged clock reading must be idempotent.");
}

mini_editor::playback_core::SequencePlaybackSnapshotPtr makeSnapshot(
    mini_editor::playback_core::SequenceId id,
    mini_editor::playback_core::FrameRate rate,
    mini_editor::playback_core::FrameCount duration)
{
    using namespace mini_editor::playback_core;

    SequencePlaybackSnapshot snapshot{
        id, SequenceRevision::initial(), rate, duration, {}, {}, {}
    };
    return std::make_shared<const SequencePlaybackSnapshot>(std::move(snapshot));
}

bool verifySequencePlaybackLifecycle()
{
    using namespace mini_editor::playback_core;

    const SequenceId sequenceId = SequenceId::create();
    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession session(PlaybackSource{SequencePreview{sequenceId}}, clock);

    auto snapshot = makeSnapshot(sequenceId, FrameRate(30, 1), FrameCount::fromFrames(300));
    if (!require(!session.applyCommand(InstallSnapshot{snapshot}, PlaybackCommandId::create()),
                 "InstallSnapshot must be accepted from Stopped.")
        || !require(session.status().phase == PlaybackPhase::Stopped,
                    "InstallSnapshot from Stopped must stay Stopped.")) {
        return false;
    }

    if (!require(!session.applyCommand(Play{}, PlaybackCommandId::create()),
                 "Play must be accepted from Stopped.")
        || !require(session.status().phase == PlaybackPhase::Playing,
                    "Play from Stopped must enter Playing.")) {
        return false;
    }

    clock.set(MasterClockTime::fromMicroseconds(1'000'000));
    if (!require(std::get<SequencePreviewStatus>(session.status().context).timelineFrame
                     == TimelineFrame::fromFrameNumber(30),
                 "Playing must advance the timeline frame from the clock, not a cache.")) {
        return false;
    }

    if (!require(!session.applyCommand(Pause{}, PlaybackCommandId::create()),
                 "Pause must be accepted from Playing.")) {
        return false;
    }
    const TimelineFrame pausedFrame =
        std::get<SequencePreviewStatus>(session.status().context).timelineFrame;
    clock.set(MasterClockTime::fromMicroseconds(5'000'000));
    if (!require(std::get<SequencePreviewStatus>(session.status().context).timelineFrame == pausedFrame,
                 "Paused position must not advance with the clock.")
        || !require(session.status().phase == PlaybackPhase::Paused,
                    "Pause from Playing must enter Paused.")) {
        return false;
    }

    if (!require(!session.applyCommand(Pause{}, PlaybackCommandId::create()),
                 "Pause from Paused must be an accepted no-op.")) {
        return false;
    }

    const PlaybackGeneration generationBeforeSeek = session.status().generation;
    if (!require(!session.applyCommand(Seek{SequenceTime::fromMicroseconds(2'000'000)},
                                       PlaybackCommandId::create()),
                 "Seek must be accepted from Paused.")
        || !require(session.status().phase == PlaybackPhase::Paused,
                    "Seek from Paused must remain Paused.")
        || !require(session.status().generation != generationBeforeSeek,
                    "Seek must advance the generation.")
        || !require(std::get<SequencePreviewStatus>(session.status().context).timelineFrame
                        == TimelineFrame::fromFrameNumber(60),
                    "Seek must move to the requested position.")) {
        return false;
    }

    if (!require(!session.applyCommand(Play{}, PlaybackCommandId::create()),
                 "Play must be accepted from Paused.")) {
        return false;
    }
    if (!require(!session.applyCommand(Seek{SequenceTime::fromMicroseconds(3'000'000)},
                                       PlaybackCommandId::create()),
                 "Seek must be accepted from Playing.")
        || !require(session.status().phase == PlaybackPhase::Playing,
                    "Seek from Playing must return to Playing.")) {
        return false;
    }

    if (!require(!session.applyCommand(Seek{SequenceTime::fromMicroseconds(1'000'000'000)},
                                       PlaybackCommandId::create()),
                 "An out-of-range seek must still be accepted (clamped).")
        || !require(std::get<SequencePreviewStatus>(session.status().context).timelineFrame
                        == TimelineFrame::fromFrameNumber(300),
                    "Seek past the snapshot end must clamp to its last frame.")) {
        return false;
    }

    const PlaybackGeneration generationBeforeStop = session.status().generation;
    if (!require(!session.applyCommand(Stop{}, PlaybackCommandId::create()),
                 "Stop must be accepted from Playing.")
        || !require(session.status().phase == PlaybackPhase::Stopped, "Stop must enter Stopped.")
        || !require(std::get<SequencePreviewStatus>(session.status().context).timelineFrame
                        == TimelineFrame::zero(),
                    "Stop must return to the sequence start.")
        || !require(session.status().generation != generationBeforeStop,
                    "Stop must advance the generation.")) {
        return false;
    }

    const PlaybackGeneration generationAtStop = session.status().generation;
    return require(!session.applyCommand(Stop{}, PlaybackCommandId::create()),
                   "Stop from Stopped must be accepted.")
        && require(session.status().generation == generationAtStop,
                   "Stop from Stopped must not advance the generation again.");
}

bool verifyFailureAndShutdownPolicy()
{
    using namespace mini_editor::playback_core;

    const SequenceId sequenceId = SequenceId::create();
    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession session(PlaybackSource{SequencePreview{sequenceId}}, clock);

    const PlaybackSessionId liveSessionId = session.status().sessionId;
    const PlaybackGeneration liveGeneration = session.status().generation;

    session.reportFailure(liveSessionId, liveGeneration.next(), PlaybackError{"stale"});
    if (!require(session.status().phase != PlaybackPhase::Failed,
                 "A stale-generation failure report must not enter Failed.")
        || !require(!session.status().error, "error must stay unset while phase is not Failed.")) {
        return false;
    }

    session.reportFailure(liveSessionId, liveGeneration, PlaybackError{"decode error"});
    if (!require(session.status().phase == PlaybackPhase::Failed,
                 "A current failure report must enter Failed.")
        || !require(session.status().error && session.status().error->message == "decode error",
                    "error must be set to exactly the reported PlaybackError.")) {
        return false;
    }

    const auto rejectPlay = session.applyCommand(Play{}, PlaybackCommandId::create());
    const auto rejectPause = session.applyCommand(Pause{}, PlaybackCommandId::create());
    const auto rejectSeek =
        session.applyCommand(Seek{SequenceTime::zero()}, PlaybackCommandId::create());
    const auto rejectRate = session.applyCommand(SetRate{200}, PlaybackCommandId::create());
    if (!require(rejectPlay && rejectPlay->reason == PlaybackRejectReason::InvalidForCurrentPhase,
                 "Play from Failed must be rejected.")
        || !require(rejectPause && rejectPause->reason == PlaybackRejectReason::InvalidForCurrentPhase,
                    "Pause from Failed must be rejected.")
        || !require(rejectSeek && rejectSeek->reason == PlaybackRejectReason::InvalidForCurrentPhase,
                    "Seek from Failed must be rejected.")
        || !require(rejectRate && rejectRate->reason == PlaybackRejectReason::InvalidForCurrentPhase,
                    "SetRate from Failed must be rejected.")) {
        return false;
    }

    if (!require(!session.applyCommand(Stop{}, PlaybackCommandId::create()),
                 "Stop from Failed must be accepted.")
        || !require(session.status().phase == PlaybackPhase::Stopped,
                    "Stop from Failed must enter Stopped.")
        || !require(!session.status().error, "Stop from Failed must clear the error.")) {
        return false;
    }

    session.reportFailure(liveSessionId, session.status().generation, PlaybackError{"second failure"});
    auto snapshot = makeSnapshot(sequenceId, FrameRate(30, 1), FrameCount::fromFrames(300));
    if (!require(!session.applyCommand(InstallSnapshot{snapshot}, PlaybackCommandId::create()),
                 "InstallSnapshot from Failed must be accepted.")
        || !require(session.status().phase == PlaybackPhase::Stopped,
                    "InstallSnapshot from Failed must enter Stopped.")
        || !require(!session.status().error, "InstallSnapshot from Failed must clear the error.")) {
        return false;
    }

    const PlaybackPhase phaseBeforeShutdown = session.status().phase;
    const PlaybackCommandId shutdownId = PlaybackCommandId::create();
    if (!require(!session.applyCommand(Shutdown{}, shutdownId), "Shutdown must be accepted.")
        || !require(session.status().phase == phaseBeforeShutdown,
                    "Shutdown's acknowledgment must not change the playback phase.")
        || !require(session.status().lastAppliedCommandId
                        && *session.status().lastAppliedCommandId == shutdownId,
                    "Shutdown must be acknowledged as the last applied command.")) {
        return false;
    }

    const auto rejectAfterShutdown = session.applyCommand(Play{}, PlaybackCommandId::create());
    return require(rejectAfterShutdown
                       && rejectAfterShutdown->reason == PlaybackRejectReason::QueueClosed,
                   "Every command after Shutdown must be rejected as QueueClosed.");
}

bool verifySourceAssetPreviewLifecycleAndKindMismatch()
{
    using namespace mini_editor::playback_core;

    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession session(PlaybackSource{SourceAssetPreview{MediaAssetId(7)}}, clock);

    const auto mismatchedSeek =
        session.applyCommand(Seek{SequenceTime::zero()}, PlaybackCommandId::create());
    if (!require(mismatchedSeek
                     && mismatchedSeek->reason == PlaybackRejectReason::SourceKindMismatch,
                 "A SequenceTime seek target on a source-asset session must be rejected as a kind mismatch.")) {
        return false;
    }

    auto snapshot = makeSnapshot(SequenceId::create(), FrameRate(30, 1), FrameCount::fromFrames(10));
    const auto mismatchedInstall =
        session.applyCommand(InstallSnapshot{snapshot}, PlaybackCommandId::create());
    if (!require(mismatchedInstall
                     && mismatchedInstall->reason == PlaybackRejectReason::SourceKindMismatch,
                 "InstallSnapshot on a source-asset session must be rejected as a kind mismatch.")) {
        return false;
    }

    if (!require(!session.applyCommand(
                     OpenSource{MediaAssetId(9), SourceTimestamp::fromMicroseconds(4'000'000),
                                SourceCompletionPolicy::HoldLastFrame},
                     PlaybackCommandId::create()),
                 "OpenSource must be accepted for a source-asset session.")) {
        return false;
    }
    const SourcePreviewStatus sourceContext =
        std::get<SourcePreviewStatus>(session.status().context);
    if (!require(sourceContext.sourceId == MediaAssetId(9),
                 "PlaybackContext must reflect the newly opened source identity.")
        || !require(sourceContext.sourceTime == sourceTimeZero(),
                    "OpenSource must seek to source-time zero.")) {
        return false;
    }

    const auto stillMismatched =
        session.applyCommand(Seek{SequenceTime::zero()}, PlaybackCommandId::create());
    if (!require(stillMismatched
                     && stillMismatched->reason == PlaybackRejectReason::SourceKindMismatch,
                 "Kind mismatch must persist across OpenSource, not only at construction.")) {
        return false;
    }

    if (!require(!session.applyCommand(Seek{SourceTimestamp::fromMicroseconds(-500)},
                                       PlaybackCommandId::create()),
                 "A negative seek target must still be accepted (clamped).")
        || !require(std::get<SourcePreviewStatus>(session.status().context).sourceTime
                        == sourceTimeZero(),
                    "A seek below the source origin must clamp to sourceTimeZero().")) {
        return false;
    }
    if (!require(!session.applyCommand(Seek{SourceTimestamp::fromMicroseconds(9'000'000)},
                                       PlaybackCommandId::create()),
                 "A too-large seek target must still be accepted (clamped).")
        || !require(std::get<SourcePreviewStatus>(session.status().context).sourceTime
                        == SourceTimestamp::fromMicroseconds(4'000'000),
                    "A seek past sourceEndTime must clamp to it.")) {
        return false;
    }

    return require(!session.status().error, "error must remain unset while phase is not Failed.");
}

bool verifySourceProgressPermille()
{
    using namespace mini_editor::playback_core;

    return require(sourceProgressPermille(SourceTimestamp::fromMicroseconds(0), sourceTimeZero()) == 0,
                   "Progress must be zero when end equals the source origin.")
        && require(sourceProgressPermille(SourceTimestamp::fromMicroseconds(-1'000'000),
                                          SourceTimestamp::fromMicroseconds(4'000'000)) == 0,
                   "Progress must clamp below zero.")
        && require(sourceProgressPermille(SourceTimestamp::fromMicroseconds(9'000'000),
                                          SourceTimestamp::fromMicroseconds(4'000'000)) == 1000,
                   "Progress must clamp above 1000.")
        && require(sourceProgressPermille(SourceTimestamp::fromMicroseconds(1'000'000),
                                          SourceTimestamp::fromMicroseconds(4'000'000)) == 250,
                   "Progress must scale linearly between zero and the end.");
}

bool verifyStaleResultRejectionAndStatusGate()
{
    using namespace mini_editor::playback_core;

    // --- Every ADR-002-listed generation-advancing trigger, on a
    // sequence-preview session. ---
    const SequenceId sequenceId = SequenceId::create();
    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession session(PlaybackSource{SequencePreview{sequenceId}}, clock);
    const PlaybackSessionId sessionId = session.status().sessionId;

    auto snapshot = makeSnapshot(sequenceId, FrameRate(30, 1), FrameCount::fromFrames(300));
    PlaybackGeneration before = session.status().generation;
    session.applyCommand(InstallSnapshot{snapshot}, PlaybackCommandId::create());
    if (!require(!session.isCurrent(sessionId, before),
                 "InstallSnapshot must advance the generation (old generation becomes stale).")) {
        return false;
    }

    before = session.status().generation;
    session.applyCommand(Seek{SequenceTime::fromMicroseconds(1'000'000)}, PlaybackCommandId::create());
    if (!require(!session.isCurrent(sessionId, before),
                 "A seek must advance the generation.")) {
        return false;
    }

    // "A newer seek supersedes older work": a second seek must again make the
    // generation from the FIRST seek stale, not just the pre-seek generation.
    before = session.status().generation;
    session.applyCommand(Seek{SequenceTime::fromMicroseconds(2'000'000)}, PlaybackCommandId::create());
    if (!require(!session.isCurrent(sessionId, before),
                 "A repeated scrubbing seek must again advance the generation.")) {
        return false;
    }

    before = session.status().generation;
    session.applyCommand(Stop{}, PlaybackCommandId::create());
    if (!require(!session.isCurrent(sessionId, before),
                 "Stop must advance the generation and discard pending work.")) {
        return false;
    }

    before = session.status().generation;
    session.applyCommand(Shutdown{}, PlaybackCommandId::create());
    if (!require(!session.isCurrent(sessionId, before),
                 "Shutdown must advance the generation and discard pending work.")) {
        return false;
    }

    // --- Source replacement (OpenSource) on a source-asset session. ---
    FakePlaybackClock sourceClock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession sourceSession(PlaybackSource{SourceAssetPreview{MediaAssetId(1)}}, sourceClock);
    const PlaybackSessionId sourceSessionId = sourceSession.status().sessionId;
    const PlaybackGeneration beforeOpen = sourceSession.status().generation;
    sourceSession.applyCommand(
        OpenSource{MediaAssetId(2), SourceTimestamp::fromMicroseconds(4'000'000),
                   SourceCompletionPolicy::HoldLastFrame},
        PlaybackCommandId::create());
    if (!require(!sourceSession.isCurrent(sourceSessionId, beforeOpen),
                 "OpenSource (source replacement) must advance the generation.")) {
        return false;
    }

    // --- A stale reportSourcePosition() must not change state; a current one
    // must, and adopting the same position twice must be idempotent. ---
    const PlaybackGeneration staleGeneration = beforeOpen;
    const PlaybackGeneration currentGeneration = sourceSession.status().generation;
    const SourceTimestamp positionBefore =
        std::get<SourcePreviewStatus>(sourceSession.status().context).sourceTime;

    sourceSession.reportSourcePosition(sourceSessionId, staleGeneration,
                                       SourceTimestamp::fromMicroseconds(1'000'000));
    if (!require(std::get<SourcePreviewStatus>(sourceSession.status().context).sourceTime
                     == positionBefore,
                 "A stale-generation source-position report must be discarded.")) {
        return false;
    }

    const StatusSequenceNumber seqBeforeAdopt = sourceSession.status().statusSeq;
    sourceSession.reportSourcePosition(sourceSessionId, currentGeneration,
                                       SourceTimestamp::fromMicroseconds(1'000'000));
    if (!require(std::get<SourcePreviewStatus>(sourceSession.status().context).sourceTime
                     == SourceTimestamp::fromMicroseconds(1'000'000),
                 "A current source-position report must be adopted.")
        || !require(sourceSession.status().statusSeq != seqBeforeAdopt,
                    "Adopting a new source position must publish a new status.")) {
        return false;
    }

    const StatusSequenceNumber seqAfterAdopt = sourceSession.status().statusSeq;
    sourceSession.reportSourcePosition(sourceSessionId, currentGeneration,
                                       SourceTimestamp::fromMicroseconds(1'000'000));
    if (!require(sourceSession.status().statusSeq == seqAfterAdopt,
                 "Reporting the same position again must be idempotent (no new status).")) {
        return false;
    }

    // --- Duplicate reportFailure() must likewise be idempotent. ---
    sourceSession.reportFailure(sourceSessionId, currentGeneration, PlaybackError{"first"});
    const StatusSequenceNumber seqAfterFirstFailure = sourceSession.status().statusSeq;
    sourceSession.reportFailure(sourceSessionId, currentGeneration, PlaybackError{"second"});
    if (!require(sourceSession.status().statusSeq == seqAfterFirstFailure,
                 "A second failure report for an already-Failed identity must be idempotent.")
        || !require(sourceSession.status().error->message == "first",
                    "An idempotent duplicate failure must not overwrite the original error.")) {
        return false;
    }

    // --- PlaybackStatusGate: monotonic within a session, resets for a new one. ---
    PlaybackStatusGate gate;
    FakePlaybackClock gateClock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession gateSession(PlaybackSource{SequencePreview{SequenceId::create()}}, gateClock);

    if (!require(gate.acceptIfNewer(gateSession.status()),
                 "The first status for a session must be accepted.")) {
        return false;
    }

    const PlaybackStatus firstStatus = gateSession.status();
    gateSession.applyCommand(Play{}, PlaybackCommandId::create());
    const PlaybackStatus secondStatus = gateSession.status();
    if (!require(gate.acceptIfNewer(secondStatus),
                 "A status with a newer statusSeq must be accepted.")
        || !require(!gate.acceptIfNewer(firstStatus),
                    "An out-of-order (older) statusSeq must be discarded.")
        || !require(!gate.acceptIfNewer(secondStatus),
                    "A repeated (non-newer) statusSeq must be discarded.")) {
        return false;
    }

    // A different session resets the comparison even if its own statusSeq
    // numbering restarts from the same values as the old session's.
    FakePlaybackClock otherClock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession otherSession(PlaybackSource{SequencePreview{SequenceId::create()}}, otherClock);
    return require(gate.acceptIfNewer(otherSession.status()),
                   "A status from a new session must be accepted even though statusSeq restarts.");
}

bool verifyPreviewPresentationCoordinator()
{
    using namespace mini_editor::playback_core;

    const SequenceId sequenceId = SequenceId::create();
    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession session(PlaybackSource{SequencePreview{sequenceId}}, clock);
    auto snapshot = makeSnapshot(sequenceId, FrameRate(30, 1), FrameCount::fromFrames(300));
    session.applyCommand(InstallSnapshot{snapshot}, PlaybackCommandId::create());

    PreviewPresentationCoordinator coordinator;

    coordinator.notifyPlaybackStatus(session.status(), /*transportJustRepositioned=*/true);
    const auto initialRequest = coordinator.currentRequest();
    if (!require(initialRequest.has_value(), "An initial Stopped status must issue a request.")
        || !require(std::holds_alternative<TransportPresentationIdentity>(initialRequest->authority),
                    "The default Stopped request must use transport authority.")) {
        return false;
    }

    const PresentationRequestId idBeforeSelection = initialRequest->requestId;
    coordinator.notifyEditingSelection(
        EditingPresentationIdentity{sequenceId, SequenceRevision::initial()},
        PresentationTarget{SequencePresentationTarget{sequenceId, SequenceRevision::initial(),
                                                       TimelineFrame::fromFrameNumber(42)}});
    const auto selectionRequest = coordinator.currentRequest();
    if (!require(selectionRequest && selectionRequest->requestId != idBeforeSelection,
                 "Clip selection must issue a new request id.")
        || !require(std::holds_alternative<EditingPresentationIdentity>(selectionRequest->authority),
                    "Clip selection must use editing authority.")) {
        return false;
    }

    coordinator.notifyPlaybackStatus(session.status(), /*transportJustRepositioned=*/false);
    if (!require(coordinator.currentRequest()->requestId == selectionRequest->requestId,
                 "An unrelated status refresh must not clear an active editing override.")) {
        return false;
    }

    session.applyCommand(Seek{SequenceTime::fromMicroseconds(1'000'000)}, PlaybackCommandId::create());
    coordinator.notifyPlaybackStatus(session.status(), /*transportJustRepositioned=*/true);
    const auto afterSeek = coordinator.currentRequest();
    if (!require(afterSeek && std::holds_alternative<TransportPresentationIdentity>(afterSeek->authority),
                 "A repositioning command must clear the editing override and restore transport authority.")
        || !require(afterSeek->requestId != selectionRequest->requestId,
                    "Clearing the override must issue a new request id.")) {
        return false;
    }

    const PresentationRequestId idAfterFirstSeek = afterSeek->requestId;
    session.applyCommand(Seek{SequenceTime::fromMicroseconds(2'000'000)}, PlaybackCommandId::create());
    coordinator.notifyPlaybackStatus(session.status(), /*transportJustRepositioned=*/true);
    if (!require(coordinator.currentRequest()->requestId != idAfterFirstSeek,
                 "Repeated scrubbing must mint a distinct request id each time.")) {
        return false;
    }

    const PresentationRequestId idBeforePlay = coordinator.currentRequest()->requestId;
    session.applyCommand(Play{}, PlaybackCommandId::create());
    coordinator.notifyPlaybackStatus(session.status(), /*transportJustRepositioned=*/true);
    if (!require(coordinator.currentRequest()->requestId != idBeforePlay,
                 "Transport resumption must mint a new request id even at an unchanged position.")
        || !require(coordinator.currentRequest()->authority
                        == PresentationAuthority{TransportPresentationIdentity{
                               session.status().sessionId, session.status().generation}},
                    "A Playing status must use transport authority naming the live session/generation.")) {
        return false;
    }

    const PresentationRequestId idWhilePlaying = coordinator.currentRequest()->requestId;
    coordinator.notifyEditingSelection(
        EditingPresentationIdentity{sequenceId, SequenceRevision::initial()},
        PresentationTarget{SequencePresentationTarget{sequenceId, SequenceRevision::initial(),
                                                       TimelineFrame::fromFrameNumber(99)}});
    if (!require(coordinator.currentRequest()->requestId == idWhilePlaying,
                 "An editing selection must be ignored while transport is active.")) {
        return false;
    }

    const PresentationRequestId idBeforeInstall = coordinator.currentRequest()->requestId;
    auto secondSnapshot = makeSnapshot(sequenceId, FrameRate(30, 1), FrameCount::fromFrames(600));
    session.applyCommand(InstallSnapshot{secondSnapshot}, PlaybackCommandId::create());
    coordinator.notifyPlaybackStatus(session.status(), /*transportJustRepositioned=*/true);
    if (!require(coordinator.currentRequest()->requestId != idBeforeInstall,
                 "Snapshot replacement must mint a new request id.")) {
        return false;
    }

    const PresentationSessionId currentSessionId = coordinator.currentRequest()->presentationSessionId;
    const PresentationRequestId currentRequestId = coordinator.currentRequest()->requestId;
    const PresentationAuthority currentAuthority = coordinator.currentRequest()->authority;
    if (!require(coordinator.isCurrentRequest(currentSessionId, currentRequestId, currentAuthority),
                 "The coordinator's own current request must be recognized as current.")) {
        return false;
    }

    PreviewPresentationCoordinator otherCoordinator;
    otherCoordinator.notifyPlaybackStatus(session.status(), true);
    const PresentationRequestId collidingRequestId = otherCoordinator.currentRequest()->requestId;
    if (!require(!coordinator.isCurrentRequest(
                     otherCoordinator.currentRequest()->presentationSessionId,
                     collidingRequestId, currentAuthority),
                 "A different presentation session must never be recognized as current, "
                 "even with a colliding request id.")) {
        return false;
    }

    coordinator.clear();
    if (!require(!coordinator.currentRequest().has_value(), "clear() must remove the current request.")) {
        return false;
    }

    coordinator.notifyPlaybackStatus(session.status(), true);
    if (!require(coordinator.currentRequest().has_value(),
                 "A status after clear() must issue a fresh request.")) {
        return false;
    }

    const auto beforeFailure = coordinator.currentRequest();
    session.reportFailure(session.status().sessionId, session.status().generation,
                          PlaybackError{"decode error"});
    coordinator.notifyPlaybackStatus(session.status(), false);
    if (!require(coordinator.currentRequest() == beforeFailure,
                 "A Failed status must retain the last accepted request rather than issuing a new one.")) {
        return false;
    }

    // Source replacement (OpenSource) on a source-asset session must also mint a new request id.
    FakePlaybackClock sourceClock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession sourceSession(PlaybackSource{SourceAssetPreview{MediaAssetId(1)}}, sourceClock);
    PreviewPresentationCoordinator sourceCoordinator;
    sourceCoordinator.notifyPlaybackStatus(sourceSession.status(), true);
    const PresentationRequestId idBeforeOpenSource = sourceCoordinator.currentRequest()->requestId;
    sourceSession.applyCommand(
        OpenSource{MediaAssetId(2), SourceTimestamp::fromMicroseconds(4'000'000),
                   SourceCompletionPolicy::HoldLastFrame},
        PlaybackCommandId::create());
    sourceCoordinator.notifyPlaybackStatus(sourceSession.status(), true);
    if (!require(sourceCoordinator.currentRequest()->requestId != idBeforeOpenSource,
                 "OpenSource (source replacement) must mint a new request id.")
        || !require(std::get<SourcePresentationTarget>(sourceCoordinator.currentRequest()->target)
                        .mediaAssetId == MediaAssetId(2),
                    "The request target must reflect the newly opened source identity.")) {
        return false;
    }

    // shutdown() removes the current request, same as clear().
    sourceCoordinator.shutdown();
    return require(!sourceCoordinator.currentRequest().has_value(),
                   "shutdown() must remove the current request.");
}

} // namespace

int main()
{
    using namespace mini_editor::playback_core;

    static_assert(CoreApiVersion::major == 1,
                  "The test documents the initial playback-core API level.");
    static_assert(!std::is_convertible<int, TimelineFrame>::value,
                  "Raw frame values must not implicitly become timeline frames.");
    static_assert(!std::is_convertible<TimelineFrame, SourceTimestamp>::value,
                  "Timeline and source time must remain separate domains.");
    static_assert(!HasAddition<SequenceTime, ClockDuration>::value,
                  "Clock duration must not be added directly to sequence time.");
    static_assert(!HasSubtraction<SourceTimestamp, TimelineFrame>::value,
                  "Source timestamps and timeline frames have no shared arithmetic.");

    if (!hasExpectedCoreApiVersion()) {
        std::cerr << "Playback core API version is not supported.\n";
        return 1;
    }

    if (!require(throwsInvalidArgumentForInvalidFrameRate(),
                 "FrameRate must reject nonpositive values.")
        || !require(FrameRate(30, 1) == FrameRate(60, 2),
                    "FrameRate must normalize rationally equal values.")
        || !require(TimelineFrame::fromFrameNumber(12)
                        - TimelineFrame::fromFrameNumber(7)
                        == FrameCount::fromFrames(5),
                    "Timeline-frame subtraction must return a frame count.")
        || !require(TimelineFrame::fromFrameNumber(7)
                        + FrameCount::fromFrames(5)
                        == TimelineFrame::fromFrameNumber(12),
                    "Timeline-frame addition must use a frame count.")
        || !verifyFrameBoundaries(FrameRate(24, 1))
        || !verifyFrameBoundaries(FrameRate(25, 1))
        || !verifyFrameBoundaries(FrameRate(30, 1))
        || !verifyFrameBoundaries(FrameRate(30'000, 1'001))
        || !verifyTwentyFourHourRange(FrameRate(24, 1))
        || !verifyTwentyFourHourRange(FrameRate(25, 1))
        || !verifyTwentyFourHourRange(FrameRate(30, 1))
        || !verifyTwentyFourHourRange(FrameRate(30'000, 1'001))
        || !verifySourceMapping()
        || !verifyRateConversions()
        || !verifyProjectRuntimeIdentityAndReadiness()
        || !verifyAnchorResolution(FrameRate(24, 1))
        || !verifyAnchorResolution(FrameRate(25, 1))
        || !verifyAnchorResolution(FrameRate(30, 1))
        || !verifyAnchorResolution(FrameRate(30'000, 1'001))
        || !verifySequencePlaybackLifecycle()
        || !verifyFailureAndShutdownPolicy()
        || !verifySourceAssetPreviewLifecycleAndKindMismatch()
        || !verifySourceProgressPermille()
        || !verifyStaleResultRejectionAndStatusGate()
        || !verifyPreviewPresentationCoordinator()) {
        return 1;
    }

    return 0;
}
