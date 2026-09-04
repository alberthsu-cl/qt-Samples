#include "PlaybackCoreBoundary.h"
#include "MediaTime.h"
#include "ProjectRuntime.h"
#include "PlaybackClock.h"
#include "PlaybackSession.h"
#include "PlaybackStatusGate.h"
#include "PreviewPresentation.h"
#include "PlaybackEngine.h"
#include "VideoWorkScheduler.h"
#include "SteadyPlaybackClock.h"
#include "PlaybackEventSink.h"

#include <thread>
#include <vector>

#include <algorithm>
#include <chrono>
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

bool verifyPlaybackEngineOrderingAndShutdown()
{
    using namespace mini_editor::playback_core;

    const SequenceId sequenceId = SequenceId::create();
    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackEngine engine(PlaybackSource{SequencePreview{sequenceId}}, clock);

    auto snapshot = makeSnapshot(sequenceId, FrameRate(30, 1), FrameCount::fromFrames(300));
    if (!require(!engine.submit(InstallSnapshot{snapshot}, PlaybackCommandId::create()),
                 "submit() must accept InstallSnapshot before shutdown.")
        || !require(!engine.submit(Seek{SequenceTime::fromMicroseconds(1'000'000)},
                                   PlaybackCommandId::create()),
                    "submit() must accept Seek before shutdown.")
        || !require(!engine.submit(Play{}, PlaybackCommandId::create()),
                    "submit() must accept Play before shutdown.")
        || !require(!engine.submit(OpenSource{MediaAssetId(1), SourceTimestamp::fromMicroseconds(0),
                                              SourceCompletionPolicy::HoldLastFrame},
                                   PlaybackCommandId::create()),
                    "submit() must accept OpenSource into the queue even though PlaybackSession "
                    "will reject it as a kind mismatch once applied.")) {
        return false;
    }

    engine.shutdownAndJoin();

    const PlaybackStatus finalStatus = engine.status();
    if (!require(finalStatus.phase == PlaybackPhase::Playing,
                 "Every command queued before shutdown must be applied, in order, before it: "
                 "InstallSnapshot -> Seek -> Play -> (rejected OpenSource) -> Shutdown must leave "
                 "phase Playing, since Shutdown does not change phase.")
        || !require(std::get<SequencePreviewStatus>(finalStatus.context).sequenceDuration
                        == FrameCount::fromFrames(300),
                    "The installed snapshot's duration must be visible in the final status.")) {
        return false;
    }

    const std::vector<PlaybackCommandRejected> rejections = engine.drainRejections();
    if (!require(rejections.size() == 1
                     && rejections.front().reason == PlaybackRejectReason::SourceKindMismatch,
                 "The queued OpenSource must be rejected on the engine thread as a kind mismatch, "
                 "observable afterward via drainRejections().")) {
        return false;
    }

    const auto rejectAfterShutdown = engine.submit(Pause{}, PlaybackCommandId::create());
    return require(rejectAfterShutdown
                       && rejectAfterShutdown->reason == PlaybackRejectReason::QueueClosed,
                   "A command submitted after shutdownAndJoin() must be rejected immediately as "
                   "QueueClosed, without needing the (already-exited) engine thread.");
}

bool verifyPlaybackEngineCrossThreadSubmission()
{
    using namespace mini_editor::playback_core;

    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackEngine engine(PlaybackSource{SourceAssetPreview{MediaAssetId(1)}}, clock);

    constexpr int kCommandsPerThread = 50;
    auto submitRange = [&engine, kCommandsPerThread](int startRate) {
        for (int i = 0; i < kCommandsPerThread; ++i)
            engine.submit(SetRate{startRate + i}, PlaybackCommandId::create());
    };

    std::thread threadA(submitRange, 1);
    std::thread threadB(submitRange, 1000);
    threadA.join();
    threadB.join();

    engine.shutdownAndJoin();

    const int finalRate = engine.status().ratePercent;
    const bool inThreadARange = finalRate >= 1 && finalRate < 1 + kCommandsPerThread;
    const bool inThreadBRange = finalRate >= 1000 && finalRate < 1000 + kCommandsPerThread;
    if (!require(inThreadARange || inThreadBRange,
                 "The final rate must be exactly one of the values submitted by either thread, "
                 "never a torn or corrupted value.")) {
        return false;
    }

    return require(engine.drainRejections().empty(),
                   "100 valid SetRate commands submitted concurrently from two threads must all "
                   "be accepted -- no rejections.");
}

// A deterministic test double for IVideoDecodeService: requestDecode()
// captures the request and callback without invoking it, so a test controls
// exactly when (and in what order) decodes complete.
class FakeVideoDecodeService final : public mini_editor::playback_core::IVideoDecodeService {
public:
    void requestDecode(mini_editor::playback_core::VideoDecodeRequest request,
                       std::function<void(mini_editor::playback_core::DecodedVideoFrame)> onDecoded) override
    {
        pending_.push_back({std::move(request), std::move(onDecoded)});
    }

    bool completeOldest(mini_editor::playback_core::DecodedVideoFrame frame)
    {
        if (pending_.empty())
            return false;
        auto entry = std::move(pending_.front());
        pending_.erase(pending_.begin());
        entry.second(std::move(frame));
        return true;
    }

    std::size_t pendingCount() const { return pending_.size(); }

    const mini_editor::playback_core::VideoDecodeRequest *oldestRequest() const
    {
        return pending_.empty() ? nullptr : &pending_.front().first;
    }

private:
    std::vector<std::pair<mini_editor::playback_core::VideoDecodeRequest,
                          std::function<void(mini_editor::playback_core::DecodedVideoFrame)>>>
        pending_;
};

// A synchronous test double for IVideoCompositor: composites inline, so
// tests don't need to separately drive composition completion.
class FakeVideoCompositor final : public mini_editor::playback_core::IVideoCompositor {
public:
    void composite(mini_editor::playback_core::DecodedVideoFrame frame,
                   mini_editor::playback_core::FramePresentationRequest request,
                   std::function<void(mini_editor::playback_core::CompositedVideoFrame)> onComposited) override
    {
        using namespace mini_editor::playback_core;

        const auto position = [&request]() -> PresentedPosition {
            if (const auto *source = std::get_if<SourcePresentationTarget>(&request.target)) {
                return PresentedPosition{PresentedSourcePosition{source->mediaAssetId, source->sourceTimestamp}};
            }
            const auto &sequence = std::get<SequencePresentationTarget>(request.target);
            return PresentedPosition{PresentedSequencePosition{
                sequence.sequenceId, sequence.sequenceRevision, sequence.timelineFrame}};
        }();

        onComposited(CompositedVideoFrame{
            request.presentationSessionId, request.requestId, request.authority, position, frame.buffer
        });
    }
};

bool verifyVideoWorkSchedulerBoundedLatestWins()
{
    using namespace mini_editor::playback_core;

    const SequenceId sequenceId = SequenceId::create();
    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession session(PlaybackSource{SequencePreview{sequenceId}}, clock);
    auto snapshot = makeSnapshot(sequenceId, FrameRate(30, 1), FrameCount::fromFrames(300));
    session.applyCommand(InstallSnapshot{snapshot}, PlaybackCommandId::create());

    PreviewPresentationCoordinator coordinator;
    coordinator.notifyPlaybackStatus(session.status(), true);

    FakeVideoDecodeService decoder;
    FakeVideoCompositor compositor;
    std::vector<CompositedVideoFrame> presented;
    VideoWorkScheduler scheduler(decoder, compositor,
                                 [&presented](CompositedVideoFrame frame) {
                                     presented.push_back(std::move(frame));
                                 });

    auto workIdentityFor = [](const PlaybackStatus &status) {
        const auto &sequence = std::get<SequencePreviewStatus>(status.context);
        return SequenceWorkIdentity{
            PlaybackWorkIdentity{status.sessionId, status.generation}, sequence.sequenceId, sequence.revision
        };
    };

    const auto requestA = coordinator.currentRequest();
    const VideoDecodeRequest decodeA{
        workIdentityFor(session.status()), MediaAssetId(1),
        SourceTimestamp::fromMicroseconds(0), MasterClockTime::fromMicroseconds(1'000'000)
    };
    scheduler.requestFrame(decodeA, *requestA);
    if (!require(scheduler.hasInFlightWork() && decoder.pendingCount() == 1,
                 "The first request must start decoding immediately.")) {
        return false;
    }

    // A second request while A is in flight becomes pending, not a second in-flight decode.
    session.applyCommand(Seek{SequenceTime::fromMicroseconds(1'000'000)}, PlaybackCommandId::create());
    coordinator.notifyPlaybackStatus(session.status(), true);
    const auto requestB = coordinator.currentRequest();
    const VideoDecodeRequest decodeB{
        workIdentityFor(session.status()), MediaAssetId(1),
        SourceTimestamp::fromMicroseconds(1'000'000), MasterClockTime::fromMicroseconds(2'000'000)
    };
    scheduler.requestFrame(decodeB, *requestB);
    if (!require(decoder.pendingCount() == 1,
                 "A second request while one is in flight must not start a second decode.")
        || !require(scheduler.hasPendingWork(), "The second request must become the pending request.")) {
        return false;
    }

    // A third request while A is still in flight replaces B as the pending request.
    session.applyCommand(Seek{SequenceTime::fromMicroseconds(2'000'000)}, PlaybackCommandId::create());
    coordinator.notifyPlaybackStatus(session.status(), true);
    const auto requestC = coordinator.currentRequest();
    const VideoDecodeRequest decodeC{
        workIdentityFor(session.status()), MediaAssetId(1),
        SourceTimestamp::fromMicroseconds(2'000'000), MasterClockTime::fromMicroseconds(3'000'000)
    };
    scheduler.requestFrame(decodeC, *requestC);
    if (!require(decoder.pendingCount() == 1 && scheduler.hasPendingWork(),
                 "A newer pending request must replace the older one, not queue alongside it.")) {
        return false;
    }

    // Completing A (the stale in-flight decode) must be discarded -- because C is pending, the
    // scheduler must move straight to decoding C, never presenting A or B.
    decoder.completeOldest(DecodedVideoFrame{decodeA.sequence, decodeA.mediaAssetId, decodeA.sourceTime,
                                             VideoFrameBuffer{1}});
    if (!require(presented.empty(), "A stale in-flight completion must not be presented.")
        || !require(!scheduler.hasPendingWork(), "Completing the in-flight decode must consume the pending slot.")
        || !require(decoder.pendingCount() == 1, "The pending request (C) must start decoding immediately.")
        || !require(decoder.oldestRequest() && *decoder.oldestRequest() == decodeC,
                    "The newly-started decode must be exactly the superseding request (C), not B.")) {
        return false;
    }

    // Completing C (the current, non-superseded decode) must be composited and presented exactly once.
    decoder.completeOldest(DecodedVideoFrame{decodeC.sequence, decodeC.mediaAssetId, decodeC.sourceTime,
                                             VideoFrameBuffer{3}});
    if (!require(presented.size() == 1, "Exactly one frame (C's) must be presented.")
        || !require(presented.front().buffer == VideoFrameBuffer{3},
                    "The presented frame must carry C's decoded payload.")
        || !require(presented.front().requestId == requestC->requestId,
                    "The presented frame's identity must match C's presentation request.")) {
        return false;
    }

    // Presentation-level staleness is a second, independent check: even a frame the scheduler
    // successfully presented is only "current" while the coordinator has not moved on since.
    if (!require(coordinator.isCurrentRequest(presented.front().presentationSessionId,
                                              presented.front().requestId, presented.front().authority),
                 "Immediately after presentation, the frame must still match the coordinator's current request.")) {
        return false;
    }
    session.applyCommand(Seek{SequenceTime::fromMicroseconds(4'000'000)}, PlaybackCommandId::create());
    coordinator.notifyPlaybackStatus(session.status(), true);
    return require(!coordinator.isCurrentRequest(presented.front().presentationSessionId,
                                                 presented.front().requestId, presented.front().authority),
                   "Once the coordinator has moved on, a previously-presented frame's identity "
                   "must no longer be recognized as current.");
}

bool verifySteadyPlaybackClock()
{
    using namespace mini_editor::playback_core;
    using namespace std::chrono_literals;

    SteadyPlaybackClock clock;
    const MasterClockTime first = clock.now();
    std::this_thread::sleep_for(10ms);
    const MasterClockTime second = clock.now();

    return require(second > first, "SteadyPlaybackClock::now() must be monotonically increasing.")
        && require((second - first) >= ClockDuration::fromMicroseconds(1'000),
                   "A 10ms sleep must advance the clock by a plausible, non-negligible amount.");
}

bool verifyUiNotificationQueueAndEngineIntegration()
{
    using namespace mini_editor::playback_core;

    UiNotificationQueue queue;
    if (!require(queue.empty(), "A freshly constructed queue must be empty.")) {
        return false;
    }
    queue.publish(PlaybackEvent{PlaybackCommandRejected{PlaybackCommandId::create(),
                                                        PlaybackRejectReason::QueueClosed}});
    if (!require(!queue.empty(), "publish() must make the queue non-empty.")) {
        return false;
    }
    const std::vector<PlaybackEvent> drained = queue.drain();
    if (!require(drained.size() == 1 && queue.empty(),
                 "drain() must return everything published and leave the queue empty.")) {
        return false;
    }

    // Integration: a PlaybackEngine wired to a sink must push every status/
    // rejection it produces, without the caller needing to poll.
    const SequenceId sequenceId = SequenceId::create();
    FakePlaybackClock fakeClock(MasterClockTime::fromMicroseconds(0));
    UiNotificationQueue engineEvents;
    PlaybackEngine engine(PlaybackSource{SequencePreview{sequenceId}}, fakeClock, &engineEvents);

    auto snapshot = makeSnapshot(sequenceId, FrameRate(30, 1), FrameCount::fromFrames(300));
    engine.submit(InstallSnapshot{snapshot}, PlaybackCommandId::create());
    engine.submit(Play{}, PlaybackCommandId::create());
    // A command PlaybackSession will reject once applied: OpenSource on a
    // sequence-mode session (SourceKindMismatch), to prove a pushed rejection
    // arrives alongside the pushed statuses, not only via drainRejections().
    engine.submit(OpenSource{MediaAssetId(1), SourceTimestamp::fromMicroseconds(0),
                             SourceCompletionPolicy::HoldLastFrame},
                 PlaybackCommandId::create());
    engine.shutdownAndJoin();

    const std::vector<PlaybackEvent> pushed = engineEvents.drain();
    // InstallSnapshot, Play, OpenSource (rejected -- pushes a status AND a
    // rejection), Shutdown: at least 5 events (4 statuses + 1 rejection).
    if (!require(pushed.size() >= 5,
                 "Every applied command must push a status, and a rejected command must also "
                 "push its PlaybackCommandRejected.")) {
        return false;
    }

    const bool sawRejection = std::any_of(pushed.begin(), pushed.end(), [](const PlaybackEvent &event) {
        return std::holds_alternative<PlaybackCommandRejected>(event)
            && std::get<PlaybackCommandRejected>(event).reason == PlaybackRejectReason::SourceKindMismatch;
    });
    const bool lastIsStatus = std::holds_alternative<PlaybackStatus>(pushed.back())
        && std::get<PlaybackStatus>(pushed.back()).phase == PlaybackPhase::Playing;
    return require(sawRejection, "The pushed events must include the OpenSource rejection.")
        && require(lastIsStatus,
                   "The final pushed event must be Shutdown's status (phase unchanged from Playing).");
}

bool verifyPlaybackEngineFailureObservation()
{
    using namespace mini_editor::playback_core;

    // --- A current failure observation, submitted through PlaybackEngine's
    // queue (not by calling PlaybackSession::reportFailure() directly),
    // must transition to Failed and be pushed to an attached event sink. ---
    FakePlaybackClock clockA(MasterClockTime::fromMicroseconds(0));
    UiNotificationQueue eventsA;
    PlaybackEngine engineA(PlaybackSource{SequencePreview{SequenceId::create()}}, clockA, &eventsA);
    const PlaybackSessionId sessionIdA = engineA.status().sessionId;
    const PlaybackGeneration generationA = engineA.status().generation;

    engineA.reportFailure(sessionIdA, generationA, PlaybackError{"decode error"});
    engineA.shutdownAndJoin();

    const PlaybackStatus statusA = engineA.status();
    if (!require(statusA.phase == PlaybackPhase::Failed,
                 "A current failure observation submitted through PlaybackEngine's queue must "
                 "transition the session to Failed.")
        || !require(statusA.error && statusA.error->message == "decode error",
                    "The Failed status must carry exactly the reported error.")) {
        return false;
    }

    const std::vector<PlaybackEvent> pushedA = eventsA.drain();
    const bool sawFailedStatus = std::any_of(pushedA.begin(), pushedA.end(),
        [](const PlaybackEvent &event) {
            return std::holds_alternative<PlaybackStatus>(event)
                && std::get<PlaybackStatus>(event).phase == PlaybackPhase::Failed;
        });
    if (!require(sawFailedStatus,
                 "A current failure observation's resulting Failed status must be pushed to the "
                 "attached event sink, the same as any other applied command's status.")) {
        return false;
    }

    // --- A stale (superseded) failure observation must be discarded, and a
    // command submitted after it must still apply normally -- proving the
    // queue processes both in submission order rather than the stale
    // observation disrupting anything around it. ---
    const SequenceId sequenceIdB = SequenceId::create();
    FakePlaybackClock clockB(MasterClockTime::fromMicroseconds(0));
    PlaybackEngine engineB(PlaybackSource{SequencePreview{sequenceIdB}}, clockB);
    const PlaybackSessionId sessionIdB = engineB.status().sessionId;
    const PlaybackGeneration staleGeneration = engineB.status().generation;

    auto snapshot = makeSnapshot(sequenceIdB, FrameRate(30, 1), FrameCount::fromFrames(300));
    engineB.submit(InstallSnapshot{snapshot}, PlaybackCommandId::create()); // advances the generation
    engineB.reportFailure(sessionIdB, staleGeneration, PlaybackError{"now stale"});
    engineB.submit(SetRate{150}, PlaybackCommandId::create());
    engineB.shutdownAndJoin();

    const PlaybackStatus statusB = engineB.status();
    if (!require(statusB.phase != PlaybackPhase::Failed,
                 "A stale-generation failure observation must not transition the session to "
                 "Failed.")
        || !require(statusB.ratePercent == 150,
                    "A command submitted after a stale failure observation must still be "
                    "applied in submission order.")) {
        return false;
    }

    // --- M4-08: TimelineEngineRouter and EngineSmokeTestSession both handle
    // a worker's mediaErrorOccurred with the exact same call shape:
    //     const PlaybackStatus current = engine_->status();
    //     engine_->reportFailure(current.sessionId, current.generation, ...);
    // i.e. one status() read, both identity fields destructured from that
    // single snapshot -- not read independently. Model that literally, for
    // both the current and the stale case. ---
    const SequenceId sequenceIdC = SequenceId::create();
    FakePlaybackClock clockC(MasterClockTime::fromMicroseconds(0));
    PlaybackEngine engineC(PlaybackSource{SequencePreview{sequenceIdC}}, clockC);

    const PlaybackStatus capturedBeforeFailureC = engineC.status(); // the "read status()" step
    engineC.reportFailure(capturedBeforeFailureC.sessionId, capturedBeforeFailureC.generation,
                          PlaybackError{"worker error, current"});
    engineC.shutdownAndJoin();
    if (!require(engineC.status().phase == PlaybackPhase::Failed,
                 "The worker-error call shape (destructure identity from one status() snapshot) "
                 "must transition to Failed when that snapshot is still current.")) {
        return false;
    }

    const SequenceId sequenceIdD = SequenceId::create();
    FakePlaybackClock clockD(MasterClockTime::fromMicroseconds(0));
    PlaybackEngine engineD(PlaybackSource{SequencePreview{sequenceIdD}}, clockD);

    const PlaybackStatus capturedBeforeFailureD = engineD.status(); // captured, then events overtake it
    auto snapshotD = makeSnapshot(sequenceIdD, FrameRate(30, 1), FrameCount::fromFrames(300));
    engineD.submit(InstallSnapshot{snapshotD}, PlaybackCommandId::create());
    engineD.submit(Play{}, PlaybackCommandId::create());
    engineD.reportFailure(capturedBeforeFailureD.sessionId, capturedBeforeFailureD.generation,
                          PlaybackError{"worker error, now stale"});
    engineD.shutdownAndJoin();
    return require(engineD.status().phase != PlaybackPhase::Failed,
                   "The worker-error call shape must discard the failure when the status() "
                   "snapshot it was built from has since gone stale, exactly like a real "
                   "corrupt-file error arriving after the user has already moved on.");
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
        || !verifyPreviewPresentationCoordinator()
        || !verifyPlaybackEngineOrderingAndShutdown()
        || !verifyPlaybackEngineCrossThreadSubmission()
        || !verifyVideoWorkSchedulerBoundedLatestWins()
        || !verifySteadyPlaybackClock()
        || !verifyUiNotificationQueueAndEngineIntegration()
        || !verifyPlaybackEngineFailureObservation()) {
        return 1;
    }

    return 0;
}
