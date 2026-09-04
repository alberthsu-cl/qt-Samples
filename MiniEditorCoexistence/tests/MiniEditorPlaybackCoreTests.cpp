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
#include "SnapshotTimelineResolver.h"
#include "SequencePreviewDriver.h"
#include "TimelineTransportView.h"
#include "PresentationDiagnostics.h"

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
    mini_editor::playback_core::FrameCount duration,
    mini_editor::playback_core::SequenceRevision revision
        = mini_editor::playback_core::SequenceRevision::initial())
{
    using namespace mini_editor::playback_core;

    SequencePlaybackSnapshot snapshot{
        id, revision, rate, duration, {}, {}, {}
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
    auto secondSnapshot = makeSnapshot(sequenceId, FrameRate(30, 1), FrameCount::fromFrames(600),
                                       SequenceRevision::initial().next());
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

// A three-clip, two-track fixture covering everything the resolver has to
// distinguish: a trimmed time-based clip, a gap, a still image, and an audio
// clip whose boundaries deliberately do not line up with the video track's.
//
//  frame   0        30       60       90
//  V1     [clip 10 ][  gap  ][clip 11 ]      10 = trimmed video, 11 = still
//  A1              [ clip 20 ]                 starts 15, ends 45
mini_editor::playback_core::SequencePlaybackSnapshot makeResolverFixture()
{
    using namespace mini_editor::playback_core;

    const SourceTimestamp trimmedIn = SourceTimestamp::fromMicroseconds(2'000'000);
    const SourceTimestamp audioIn = SourceTimestamp::fromMicroseconds(0);

    return SequencePlaybackSnapshot {
        SequenceId::create(), SequenceRevision::initial(), FrameRate(30, 1),
        FrameCount::fromFrames(90),
        {
            { 1, PlaybackMediaKind::Video, "v1.mp4", MediaAvailability::Available,
              SourceTimestamp::fromMicroseconds(10'000'000) },
            { 2, PlaybackMediaKind::Image, "still.png", MediaAvailability::Available,
              std::nullopt },
            { 3, PlaybackMediaKind::Audio, "a1.wav", MediaAvailability::Available,
              SourceTimestamp::fromMicroseconds(10'000'000) }
        },
        {
            { 10, 1, PlaybackTrackType::Video, TimelineFrame::fromFrameNumber(0),
              FrameCount::fromFrames(30), trimmedIn, {} },
            { 11, 2, PlaybackTrackType::Video, TimelineFrame::fromFrameNumber(60),
              FrameCount::fromFrames(30), std::nullopt, {} }
        },
        {
            { 20, 3, PlaybackTrackType::Audio, TimelineFrame::fromFrameNumber(15),
              FrameCount::fromFrames(30), audioIn, {} }
        },
        false
    };
}

bool verifySnapshotTimelineResolverBoundaries()
{
    using namespace mini_editor::playback_core;

    const SequencePlaybackSnapshot snapshot = makeResolverFixture();
    auto at = [&snapshot](std::int64_t frame) {
        return SnapshotTimelineResolver::resolve(
            snapshot, TimelineFrame::fromFrameNumber(frame));
    };

    // First frame of clip 10: video only, because A1 has not started yet.
    const ResolvedSnapshotFrame first = at(0);
    if (!require(first.video && first.video->clipId == 10
                     && first.video->clipLocalFrame == FrameCount::zero()
                     && !first.audio,
                 "The resolver must return the clip covering the first frame, and "
                 "nothing on a track whose clip has not started."))
        return false;

    // Both tracks at once, each with its own clip-local frame.
    const ResolvedSnapshotFrame both = at(20);
    if (!require(both.video && both.video->clipId == 10
                     && both.video->clipLocalFrame == FrameCount::fromFrames(20)
                     && both.audio && both.audio->clipId == 20
                     && both.audio->clipLocalFrame == FrameCount::fromFrames(5),
                 "V1 and A1 must resolve independently, each against its own "
                 "clip start."))
        return false;

    // The exclusive end boundary: frame 30 belongs to the gap, not clip 10.
    const ResolvedSnapshotFrame lastCovered = at(29);
    const ResolvedSnapshotFrame firstUncovered = at(30);
    if (!require(lastCovered.video && lastCovered.video->clipId == 10
                     && !firstUncovered.video,
                 "A clip must cover [start, start + duration) exactly: its last "
                 "frame resolves, the frame after it does not."))
        return false;

    // A1-only, in the middle of V1's gap.
    if (!require(!at(40).video && at(40).audio && at(40).audio->clipId == 20,
                 "A gap on one track must not suppress the other track."))
        return false;

    // A gap on both tracks is a resolved frame with no media, not a failure.
    const ResolvedSnapshotFrame emptyGap = at(50);
    return require(!emptyGap.video && !emptyGap.audio
                       && emptyGap.timelineFrame == TimelineFrame::fromFrameNumber(50),
                   "A frame covered by no clip must resolve to a frame with no "
                   "media rather than to nothing at all.");
}

bool verifySnapshotTimelineResolverSourceTime()
{
    using namespace mini_editor::playback_core;

    const SequencePlaybackSnapshot snapshot = makeResolverFixture();
    auto at = [&snapshot](std::int64_t frame) {
        return SnapshotTimelineResolver::resolve(
            snapshot, TimelineFrame::fromFrameNumber(frame));
    };

    // Trimmed source-in: clip-local elapsed time is added to the trim point,
    // never to zero. At 30 fps frame 29 starts at ceil(29e6/30) = 966'667 us.
    const ResolvedSnapshotFrame trimmed = at(29);
    if (!require(trimmed.video && trimmed.video->sourceTime
                     && *trimmed.video->sourceTime
                         == SourceTimestamp::fromMicroseconds(2'966'667),
                 "A trimmed clip's source time must be its source-in plus the "
                 "clip-local elapsed time."))
        return false;

    // The same relationship, stated compositionally rather than as a constant.
    const ResolvedSnapshotFrame midAudio = at(25);
    const std::int64_t expectedAudioUs =
        sequenceTimeAtFrameStart(TimelineFrame::fromFrameNumber(10), FrameRate(30, 1))
            .microsecondsForAdapter();
    if (!require(midAudio.audio && midAudio.audio->sourceTime
                     && midAudio.audio->sourceTime->microsecondsForAdapter()
                         == expectedAudioUs,
                 "An untrimmed clip's source time must equal the clip-local "
                 "elapsed time."))
        return false;

    // A still image has no source timeline, so no frame of it has a source
    // time -- including the last one, where a naive offset would run past the
    // end of a zero-length source.
    const ResolvedSnapshotFrame stillStart = at(60);
    const ResolvedSnapshotFrame stillEnd = at(89);
    return require(stillStart.video && stillStart.video->clipId == 11
                       && stillStart.video->mediaKind == PlaybackMediaKind::Image
                       && !stillStart.video->sourceTime
                       && stillEnd.video && !stillEnd.video->sourceTime
                       && stillEnd.video->clipLocalFrame == FrameCount::fromFrames(29),
                   "A still image must resolve with a clip-local frame but no "
                   "source time, at every frame it covers.");
}

bool verifySnapshotTimelineResolverEdgeCases()
{
    using namespace mini_editor::playback_core;

    // An empty snapshot resolves rather than crashing or reporting an error.
    const SequencePlaybackSnapshot empty {
        SequenceId::create(), SequenceRevision::initial(), FrameRate(30, 1),
        FrameCount::zero(), {}, {}, {}, false
    };
    const ResolvedSnapshotFrame emptyResult =
        SnapshotTimelineResolver::resolve(empty, TimelineFrame::fromFrameNumber(0));
    if (!require(!emptyResult.video && !emptyResult.audio,
                 "An empty snapshot must resolve to a frame with no media."))
        return false;

    const SequencePlaybackSnapshot snapshot = makeResolverFixture();

    // Past the end of the sequence.
    const ResolvedSnapshotFrame past = SnapshotTimelineResolver::resolve(
        snapshot, TimelineFrame::fromFrameNumber(90));
    if (!require(!past.video && !past.audio,
                 "A frame at or past the sequence duration must resolve to no "
                 "media."))
        return false;

    // The legacy resolver clamps a negative raw int to zero. Nothing here
    // needs to: TimelineFrame will not construct from a negative number, so
    // the resolver's whole input domain is already non-negative.
    static_assert(!std::is_constructible<TimelineFrame, std::int64_t>::value,
                  "TimelineFrame must be built through its checked factory.");

    // A clip whose media descriptor is missing resolves to nothing on that
    // track: better a visibly blank track than a decode request naming a file
    // the snapshot never described.
    SequencePlaybackSnapshot missingMedia = makeResolverFixture();
    missingMedia.media.erase(missingMedia.media.begin());
    const ResolvedSnapshotFrame orphaned = SnapshotTimelineResolver::resolve(
        missingMedia, TimelineFrame::fromFrameNumber(0));
    if (!require(!orphaned.video,
                 "A clip with no media descriptor must resolve to no media on "
                 "its track."))
        return false;

    // Availability is reported, not filtered: the caller decides whether a
    // missing file is a failure (ADR-002 keeps that decision in the session).
    SequencePlaybackSnapshot unavailable = makeResolverFixture();
    unavailable.media.front().availability = MediaAvailability::Unavailable;
    const ResolvedSnapshotFrame offline = SnapshotTimelineResolver::resolve(
        unavailable, TimelineFrame::fromFrameNumber(0));
    return require(offline.video
                       && offline.video->availability == MediaAvailability::Unavailable
                       && offline.video->immutableSourceLocator == "v1.mp4",
                   "Unavailable media must still resolve, with its availability "
                   "reported to the caller.");
}

bool verifySnapshotInstallRevisionRules()
{
    using namespace mini_editor::playback_core;

    const SequenceId sequenceId = SequenceId::create();
    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession session(PlaybackSource{SequencePreview{sequenceId}}, clock);

    const SequenceRevision first = SequenceRevision::initial();
    const SequenceRevision second = first.next();
    auto install = [&session, sequenceId](SequenceRevision revision, std::int64_t frames) {
        return session.applyCommand(
            InstallSnapshot{makeSnapshot(sequenceId, FrameRate(30, 1),
                                         FrameCount::fromFrames(frames), revision)},
            PlaybackCommandId::create());
    };

    if (!require(!install(first, 300),
                 "The first snapshot for a sequence must be accepted at any revision."))
        return false;

    // Move off zero so a rolled-back install would be visible as a position
    // change and not just as different content.
    session.applyCommand(Seek{sequenceTimeAtFrameStart(TimelineFrame::fromFrameNumber(120),
                                          FrameRate(30, 1))}, PlaybackCommandId::create());
    const PlaybackStatus beforeStale = session.status();

    // A duplicate install of the same revision.
    const std::optional<PlaybackCommandRejected> duplicate = install(first, 30);
    if (!require(duplicate
                     && duplicate->reason == PlaybackRejectReason::StaleSequenceRevision,
                 "Re-installing the same revision must be rejected as stale."))
        return false;

    const PlaybackStatus afterStale = session.status();
    if (!require(afterStale.generation == beforeStale.generation
                     && afterStale.statusSeq == beforeStale.statusSeq
                     && std::get<SequencePreviewStatus>(afterStale.context).sequenceDuration
                         == FrameCount::fromFrames(300)
                     && std::get<SequencePreviewStatus>(afterStale.context).timelineFrame
                         == TimelineFrame::fromFrameNumber(120),
                 "A rejected install must change nothing: not the generation, not the "
                 "status sequence, not the installed content, not the position."))
        return false;

    // Out-of-order: revision 2 lands, then revision 1 arrives late.
    if (!require(!install(second, 600), "A strictly newer revision must be accepted."))
        return false;
    const std::optional<PlaybackCommandRejected> outOfOrder = install(first, 30);
    if (!require(outOfOrder
                     && outOfOrder->reason == PlaybackRejectReason::StaleSequenceRevision,
                 "An out-of-order install must be rejected rather than roll content back."))
        return false;

    // An accepted same-sequence install is an edit, not new content, so it
    // keeps the playhead where the user left it.
    return require(std::get<SequencePreviewStatus>(session.status().context).timelineFrame
                       == TimelineFrame::fromFrameNumber(120)
                   && std::get<SequencePreviewStatus>(session.status().context).sequenceDuration
                       == FrameCount::fromFrames(600),
                   "Installing a newer revision of the same sequence must keep the "
                   "current position and adopt the new content.");
}

bool verifySessionRetargetAcrossProjectReload()
{
    using namespace mini_editor::playback_core;

    // The M4-06 defect this covers: the engine's SequencePreview identity was
    // fixed at construction, but replaceProject() mints a fresh SequenceId, so
    // after any project load the session's identity no longer matched the
    // snapshots being installed into it.
    const SequenceId beforeReload = SequenceId::create();
    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession session(PlaybackSource{SequencePreview{beforeReload}}, clock);

    session.applyCommand(
        InstallSnapshot{makeSnapshot(beforeReload, FrameRate(30, 1),
                                     FrameCount::fromFrames(300))},
        PlaybackCommandId::create());
    session.applyCommand(Seek{sequenceTimeAtFrameStart(TimelineFrame::fromFrameNumber(90),
                                          FrameRate(30, 1))}, PlaybackCommandId::create());
    session.applyCommand(Play{}, PlaybackCommandId::create());
    const PlaybackStatus playing = session.status();

    // A reload never reuses the previous runtime SequenceId (ADR-006 rule 2),
    // and its first snapshot may carry any revision -- including one that
    // would be stale if it were compared against the outgoing sequence's.
    const SequenceId afterReload = SequenceId::create();
    if (!require(!session.applyCommand(
                     InstallSnapshot{makeSnapshot(afterReload, FrameRate(25, 1),
                                                  FrameCount::fromFrames(120))},
                     PlaybackCommandId::create()),
                 "The first snapshot of a newly introduced sequence must be accepted "
                 "even while the previous sequence is playing."))
        return false;

    const PlaybackStatus reloaded = session.status();
    const auto &context = std::get<SequencePreviewStatus>(reloaded.context);
    if (!require(context.sequenceId == afterReload
                     && context.sequenceId != beforeReload
                     && context.frameRate == FrameRate(25, 1)
                     && context.sequenceDuration == FrameCount::fromFrames(120),
                 "After a reload the session's reported identity must be the installed "
                 "snapshot's, not the one it was constructed with."))
        return false;

    if (!require(reloaded.phase == PlaybackPhase::Stopped
                     && context.timelineFrame == TimelineFrame::zero(),
                 "A different sequence is different content, so the transport must "
                 "start over rather than clamp a position measured against the "
                 "timeline that was just replaced."))
        return false;

    // ADR-006: a reload preserves PlaybackSessionId when the engine session
    // continues to run, and advances the generation -- which is what makes
    // every piece of work still in flight for the old sequence stale.
    if (!require(reloaded.sessionId == playing.sessionId
                     && !session.isCurrent(playing.sessionId, playing.generation)
                     && session.isCurrent(reloaded.sessionId, reloaded.generation),
                 "A reload must keep the session id, advance the generation, and make "
                 "the pre-reload identity stale."))
        return false;

    // Retargeting also clears a failure: the file that could not be decoded
    // belongs to a project that is no longer open.
    session.reportFailure(reloaded.sessionId, reloaded.generation,
                          PlaybackError{"decoder failed on the reloaded project"});
    if (!require(session.status().phase == PlaybackPhase::Failed,
                 "The test's own failure setup must reach Failed."))
        return false;

    const SequenceId afterSecondReload = SequenceId::create();
    session.applyCommand(
        InstallSnapshot{makeSnapshot(afterSecondReload, FrameRate(30, 1),
                                     FrameCount::fromFrames(300))},
        PlaybackCommandId::create());
    return require(session.status().phase == PlaybackPhase::Stopped
                       && !session.status().error
                       && std::get<SequencePreviewStatus>(session.status().context).sequenceId
                           == afterSecondReload,
                   "Loading another project after a failure must clear the error and "
                   "retarget onto the new sequence.");
}

// Everything the driver has to get right, in one timeline: two video clips
// with a gap between them, then a still. Same shape as the resolver fixture,
// but tied to a real session's sequence so the statuses fed in are genuine.
//
//  frame   0        30       60       90      120
//  V1     [clip 10 ][  gap  ][clip 11 ][clip 12]
//  A1          [-------- clip 20 -------]
//                            11 = second video, 12 = still image
//
// A1 deliberately starts and ends on neither of V1's boundaries, and spans
// V1's gap: nothing about one lane's clip changes may disturb the other.
mini_editor::playback_core::SequencePlaybackSnapshotPtr makeDriverFixture(
    mini_editor::playback_core::SequenceId sequenceId,
    mini_editor::playback_core::MediaAvailability firstClipAvailability
        = mini_editor::playback_core::MediaAvailability::Available,
    bool isVideoTrackMuted = false)
{
    using namespace mini_editor::playback_core;

    SequencePlaybackSnapshot snapshot {
        sequenceId, SequenceRevision::initial(), FrameRate(30, 1),
        FrameCount::fromFrames(120),
        {
            { 1, PlaybackMediaKind::Video, "first.mp4", firstClipAvailability,
              SourceTimestamp::fromMicroseconds(10'000'000) },
            { 2, PlaybackMediaKind::Video, "second.mp4", MediaAvailability::Available,
              SourceTimestamp::fromMicroseconds(10'000'000) },
            { 3, PlaybackMediaKind::Image, "still.png", MediaAvailability::Available,
              std::nullopt },
            { 4, PlaybackMediaKind::Audio, "voice.wav", MediaAvailability::Available,
              SourceTimestamp::fromMicroseconds(10'000'000) }
        },
        {
            { 10, 1, PlaybackTrackType::Video, TimelineFrame::fromFrameNumber(0),
              FrameCount::fromFrames(30),
              SourceTimestamp::fromMicroseconds(2'000'000), {} },
            { 11, 2, PlaybackTrackType::Video, TimelineFrame::fromFrameNumber(60),
              FrameCount::fromFrames(30), SourceTimestamp::fromMicroseconds(0), {} },
            { 12, 3, PlaybackTrackType::Video, TimelineFrame::fromFrameNumber(90),
              FrameCount::fromFrames(30), std::nullopt, {} }
        },
        {
            // 60 frames with a 10-frame ramp at each end, so the fade gain is
            // a value the test can predict rather than just "not 100".
            { 20, 4, PlaybackTrackType::Audio, TimelineFrame::fromFrameNumber(15),
              FrameCount::fromFrames(60), SourceTimestamp::fromMicroseconds(0),
              { 100, 100, 10, 10, 0, 100 } }
        },
        isVideoTrackMuted
    };
    return std::make_shared<const SequencePlaybackSnapshot>(std::move(snapshot));
}

// Everything a driver test needs, assembled once: a session producing real
// statuses, the coordinator and scheduler the driver drives, and the fake
// ports underneath them.
struct DriverHarness final {
    explicit DriverHarness(
        mini_editor::playback_core::MediaAvailability firstClipAvailability
            = mini_editor::playback_core::MediaAvailability::Available)
        : sequenceId(mini_editor::playback_core::SequenceId::create())
        , clock(mini_editor::playback_core::MasterClockTime::fromMicroseconds(0))
        , session(mini_editor::playback_core::PlaybackSource{
                      mini_editor::playback_core::SequencePreview{sequenceId}}, clock)
        , scheduler(decoder, compositor,
                    [this](mini_editor::playback_core::CompositedVideoFrame frame) {
                        presented.push_back(std::move(frame));
                    })
        , driver(coordinator, scheduler, clock)
    {
        using namespace mini_editor::playback_core;
        snapshot = makeDriverFixture(sequenceId, firstClipAvailability);
        session.applyCommand(InstallSnapshot{snapshot}, PlaybackCommandId::create());
        driver.installSnapshot(snapshot);
    }

    // Moves the transport to one frame and lets the driver react, the way the
    // router does for a status arriving from the engine.
    mini_editor::playback_core::PreviewDriveOutcome seekTo(std::int64_t frame)
    {
        using namespace mini_editor::playback_core;
        session.applyCommand(
            Seek{sequenceTimeAtFrameStart(TimelineFrame::fromFrameNumber(frame),
                                          FrameRate(30, 1))},
            PlaybackCommandId::create());
        return driver.notifyPlaybackStatus(session.status(),
                                           /*transportJustRepositioned=*/true);
    }

    // The other way the driver is called: sampling a free-running transport,
    // which is the only thing that notices a clip boundary while playing.
    mini_editor::playback_core::PreviewDriveOutcome tick()
    {
        return driver.notifyPlaybackStatus(session.status(),
                                           /*transportJustRepositioned=*/false);
    }

    mini_editor::playback_core::SequenceId sequenceId;
    FakePlaybackClock clock;
    mini_editor::playback_core::PlaybackSession session;
    mini_editor::playback_core::SequencePlaybackSnapshotPtr snapshot;
    mini_editor::playback_core::PreviewPresentationCoordinator coordinator;
    FakeVideoDecodeService decoder;
    FakeVideoCompositor compositor;
    std::vector<mini_editor::playback_core::CompositedVideoFrame> presented;
    mini_editor::playback_core::VideoWorkScheduler scheduler;
    mini_editor::playback_core::SequencePreviewDriver driver;
};

bool verifyPreviewDriverFollowsThePlayheadAcrossClips()
{
    using namespace mini_editor::playback_core;

    DriverHarness harness;

    const PreviewDriveOutcome atStart = harness.seekTo(0);
    if (!require(atStart.openClip && atStart.openClip->clipId == 10
                     && atStart.openClip->immutableSourceLocator == "first.mp4"
                     && !atStart.showNothing,
                 "The first status must open the clip under the playhead."))
        return false;

    // Inside the same clip nothing may be re-opened: re-opening a file every
    // frame is exactly the stall this issue exists to avoid.
    if (!require(!harness.seekTo(10).openClip && !harness.seekTo(29).openClip,
                 "Moving within one clip must not re-open its media."))
        return false;

    // The gap. Not a failure and not an error -- there is simply nothing to
    // show, and the driver must say so rather than leave the previous clip up.
    const PreviewDriveOutcome inGap = harness.seekTo(30);
    if (!require(inGap.showNothing && !inGap.openClip
                     && !harness.driver.openClipId(),
                 "A gap must blank the viewport and forget the open clip."))
        return false;

    // Crossing into the second clip.
    const PreviewDriveOutcome secondClip = harness.seekTo(60);
    if (!require(secondClip.openClip && secondClip.openClip->clipId == 11
                     && secondClip.openClip->immutableSourceLocator == "second.mp4"
                     && secondClip.openClip->sourceTime
                     && *secondClip.openClip->sourceTime
                         == SourceTimestamp::fromMicroseconds(0),
                 "Crossing a clip boundary must open the new clip at its source in."))
        return false;

    // Mid-clip, the source time follows the playhead rather than restarting.
    const PreviewDriveOutcome midSecond = harness.seekTo(75);
    if (!require(!midSecond.openClip && harness.driver.openClipId() == 11,
                 "The second clip must stay open as the playhead moves through it."))
        return false;

    // Going back to the first clip is a boundary crossing too.
    const PreviewDriveOutcome backToFirst = harness.seekTo(5);
    if (!require(backToFirst.openClip && backToFirst.openClip->clipId == 10,
                 "Seeking backwards across a boundary must re-open the earlier clip."))
        return false;

    // Back through the gap and out the other side: leaving a clip forgets it,
    // so re-entering it is a boundary crossing again rather than a no-op.
    harness.seekTo(45);
    const PreviewDriveOutcome reentered = harness.seekTo(60);
    return require(reentered.openClip && reentered.openClip->clipId == 11
                       && harness.driver.openClipId() == 11,
                   "Re-entering a clip after a gap must open it again.");
}

bool verifyPreviewDriverBoundsScrubbingAndSkipsStills()
{
    using namespace mini_editor::playback_core;

    DriverHarness harness;

    // Scrubbing while paused is the request/response case: each new position
    // asks for a decode, and the scheduler bounds them to one in flight plus
    // one pending.
    harness.seekTo(0);
    if (!require(harness.decoder.pendingCount() == 1 && harness.scheduler.hasInFlightWork(),
                 "A paused/stopped position must request a decode."))
        return false;

    harness.seekTo(5);
    harness.seekTo(10);
    harness.seekTo(15);
    if (!require(harness.decoder.pendingCount() == 1 && harness.scheduler.hasPendingWork(),
                 "Three further scrub positions must collapse to one pending decode, "
                 "not three queued ones."))
        return false;

    // The in-flight decode for frame 0 finishes after frame 15 superseded it.
    // ADR-003: the worker does not decide its own currency, so the scheduler
    // discards the result rather than presenting a frame the user has already
    // scrubbed past.
    const VideoDecodeRequest stale = *harness.decoder.oldestRequest();
    harness.decoder.completeOldest(DecodedVideoFrame{
        stale.sequence, stale.mediaAssetId, stale.sourceTime, VideoFrameBuffer{7, nullptr}});
    const bool stalePresented = std::any_of(
        harness.presented.begin(), harness.presented.end(),
        [](const CompositedVideoFrame &frame) { return frame.buffer.placeholderPixelChecksum == 7; });
    if (!require(!stalePresented,
                 "A decode that completes after a newer scrub position superseded it "
                 "must be discarded, not presented."))
        return false;

    // The other half of the same rule: discarding the superseded result is
    // what lets the newest one start, and that one does reach presentation.
    // Without this the assertion above would also pass if nothing ever did.
    const VideoDecodeRequest newest = *harness.decoder.oldestRequest();
    harness.decoder.completeOldest(DecodedVideoFrame{
        newest.sequence, newest.mediaAssetId, newest.sourceTime, VideoFrameBuffer{9, nullptr}});
    const bool newestPresented = std::any_of(
        harness.presented.begin(), harness.presented.end(),
        [](const CompositedVideoFrame &frame) { return frame.buffer.placeholderPixelChecksum == 9; });
    if (!require(newestPresented && harness.presented.size() == 1,
                 "The newest scrub position's decode must be the one -- and the only "
                 "one -- that reaches presentation."))
        return false;

    // A still image has no source timeline. It is opened for presentation but
    // never handed to a decoder -- which is what stops a picture from coming
    // back as a decode error and taking the session to Failed.
    const std::size_t decodesBeforeStill = harness.decoder.pendingCount();
    const PreviewDriveOutcome still = harness.seekTo(95);
    if (!require(still.openClip && still.openClip->clipId == 12
                     && still.openClip->mediaKind == PlaybackMediaKind::Image
                     && !still.openClip->sourceTime
                     && harness.decoder.pendingCount() == decodesBeforeStill,
                 "A still must be opened for presentation but never requested from a "
                 "decoder."))
        return false;

    // While playing, the adapter's continuous player is already producing
    // frames; a second, request/response producer for the same viewport would
    // be two sources of truth.
    harness.seekTo(0);
    const std::size_t decodesBeforePlay = harness.decoder.pendingCount();
    harness.session.applyCommand(Play{}, PlaybackCommandId::create());
    harness.driver.notifyPlaybackStatus(harness.session.status(), true);
    harness.clock.set(MasterClockTime::fromMicroseconds(500'000));
    harness.tick();
    return require(harness.decoder.pendingCount() == decodesBeforePlay,
                   "Playing must not request decodes: the continuous player is already "
                   "producing frames.");
}

bool verifyPreviewDriverHoldsWhenItCannotResolve()
{
    using namespace mini_editor::playback_core;

    // Media the snapshot could not find is not a failure -- it is nothing to
    // show, and it must not reach a decoder either.
    DriverHarness unavailable(MediaAvailability::Unavailable);
    const PreviewDriveOutcome missing = unavailable.seekTo(0);
    if (!require(missing.showNothing && !missing.openClip
                     && unavailable.decoder.pendingCount() == 0,
                 "Unavailable media must blank the viewport without asking a decoder "
                 "for anything."))
        return false;

    DriverHarness harness;
    harness.seekTo(0);

    // A status describing a sequence the driver has no snapshot for resolves
    // nothing. Guessing in between is how a wrong-clip frame reaches the
    // viewport during a project reload.
    const SequenceId otherSequence = SequenceId::create();
    FakePlaybackClock otherClock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession otherSession(PlaybackSource{SequencePreview{otherSequence}}, otherClock);
    otherSession.applyCommand(
        InstallSnapshot{makeDriverFixture(otherSequence)}, PlaybackCommandId::create());
    const PreviewDriveOutcome foreign =
        harness.driver.notifyPlaybackStatus(otherSession.status(), true);
    if (!require(!foreign.openClip && !foreign.showNothing
                     && harness.driver.openClipId() == 10,
                 "A status for another sequence must change nothing, not blank the "
                 "viewport."))
        return false;

    // ADR-003 retains the last accepted frame on failure. Blanking here would
    // take away the picture at exactly the moment the user needs to see what
    // was playing when it broke.
    const PlaybackStatus current = harness.session.status();
    harness.session.reportFailure(current.sessionId, current.generation,
                                  PlaybackError{"decoder failed"});
    const PreviewDriveOutcome failed =
        harness.driver.notifyPlaybackStatus(harness.session.status(), true);
    return require(!failed.openClip && !failed.showNothing
                       && harness.driver.openClipId() == 10,
                   "A failed session must hold the last frame rather than blank the "
                   "viewport or switch clips.");
}

bool verifyTimelineTransportView()
{
    using namespace mini_editor::playback_core;

    const SequenceId sequenceId = SequenceId::create();
    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession session(PlaybackSource{SequencePreview{sequenceId}}, clock);
    session.applyCommand(
        InstallSnapshot{makeSnapshot(sequenceId, FrameRate(30, 1), FrameCount::fromFrames(300))},
        PlaybackCommandId::create());

    const auto stopped = timelineTransportViewFor(session.status());
    if (!require(stopped && !stopped->isPlaying && !stopped->isPaused
                     && stopped->timelineFrame == 0
                     && stopped->durationFrames == 300
                     && stopped->framesPerSecond == 30
                     && stopped->playbackRatePercent == 100,
                 "A stopped sequence session must publish a stopped transport view "
                 "carrying the snapshot's duration and rate."))
        return false;

    session.applyCommand(Play{}, PlaybackCommandId::create());
    clock.set(MasterClockTime::fromMicroseconds(1'000'000));
    const auto playing = timelineTransportViewFor(session.status());
    if (!require(playing && playing->isPlaying && !playing->isPaused
                     && playing->timelineFrame == 30,
                 "A playing view must report the frame the clock resolves to, not a "
                 "cached one."))
        return false;

    session.applyCommand(Pause{}, PlaybackCommandId::create());
    const auto paused = timelineTransportViewFor(session.status());
    if (!require(paused && !paused->isPlaying && paused->isPaused
                     && paused->timelineFrame == 30,
                 "Paused must be distinguishable from stopped, holding its frame."))
        return false;

    session.applyCommand(SetRate{50}, PlaybackCommandId::create());
    if (!require(timelineTransportViewFor(session.status())->playbackRatePercent == 50,
                 "The transport rate must reach the view."))
        return false;

    // A failure is neither playing nor paused, and does not invent a phase.
    const PlaybackStatus current = session.status();
    session.reportFailure(current.sessionId, current.generation, PlaybackError{"broken"});
    const auto failed = timelineTransportViewFor(session.status());
    if (!require(failed && !failed->isPlaying && !failed->isPaused,
                 "A failed session must publish neither playing nor paused."))
        return false;

    // 30000/1001 has to paint as 30, not 29: PlaybackState carries a whole
    // number and truncation would show the wrong rate for every NTSC project.
    const SequenceId ntscId = SequenceId::create();
    FakePlaybackClock ntscClock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession ntsc(PlaybackSource{SequencePreview{ntscId}}, ntscClock);
    ntsc.applyCommand(
        InstallSnapshot{makeSnapshot(ntscId, FrameRate(30'000, 1'001), FrameCount::fromFrames(90))},
        PlaybackCommandId::create());
    if (!require(timelineTransportViewFor(ntsc.status())->framesPerSecond == 30,
                 "A rational frame rate must round to the nearest whole rate."))
        return false;

    // A source-asset session has no timeline transport to paint. Inventing
    // one would put two meanings on the same cache.
    FakePlaybackClock sourceClock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession sourceSession(
        PlaybackSource{SourceAssetPreview{MediaAssetId(4)}}, sourceClock);
    return require(!timelineTransportViewFor(sourceSession.status()),
                   "A source-asset session must publish no timeline transport view.");
}

bool verifyFramePresentedIsDiagnosticsOnly()
{
    using namespace mini_editor::playback_core;

    const SequenceId sequenceId = SequenceId::create();
    FakePlaybackClock clock(MasterClockTime::fromMicroseconds(0));
    PlaybackSession session(PlaybackSource{SequencePreview{sequenceId}}, clock);
    session.applyCommand(
        InstallSnapshot{makeSnapshot(sequenceId, FrameRate(30, 1), FrameCount::fromFrames(300))},
        PlaybackCommandId::create());
    session.applyCommand(
        Seek{sequenceTimeAtFrameStart(TimelineFrame::fromFrameNumber(45), FrameRate(30, 1))},
        PlaybackCommandId::create());

    PreviewPresentationCoordinator coordinator;
    coordinator.notifyPlaybackStatus(session.status(), true);
    const FramePresentationRequest request = *coordinator.currentRequest();

    // The position a renderer acknowledges is the position the request asked
    // for, in the domain the request named.
    const PresentedPosition position = presentedPositionFor(request.target);
    const auto *sequencePosition = std::get_if<PresentedSequencePosition>(&position);
    if (!require(sequencePosition != nullptr
                     && sequencePosition->sequenceId == sequenceId
                     && sequencePosition->timelineFrame == TimelineFrame::fromFrameNumber(45),
                 "A sequence request must map to a sequence presented position at the "
                 "same frame."))
        return false;

    PresentationDiagnostics diagnostics;
    diagnostics.recordComposited(CompositedVideoFrame{
        request.presentationSessionId, request.requestId, request.authority, position,
        VideoFrameBuffer{3, nullptr}});
    if (!require(diagnostics.compositedCount() == 1 && diagnostics.presentedCount() == 0
                     && !diagnostics.lastPresented(),
                 "ADR-003 criterion 12: a frame that composited but never reached a "
                 "surface must not count as presented."))
        return false;

    // The status a renderer acknowledgement must not be able to change.
    const PlaybackStatus before = session.status();
    diagnostics.recordPresented(FramePresented{
        request.presentationSessionId, request.requestId, request.authority, position});
    const PlaybackStatus after = session.status();

    if (!require(diagnostics.presentedCount() == 1 && diagnostics.compositedCount() == 1
                     && diagnostics.lastPresented()
                     && diagnostics.lastPresented()->requestId == request.requestId,
                 "Presentation must be counted separately from composition readiness "
                 "and keep the acknowledged request identity."))
        return false;

    // FramePresented carries no clock and no session reference, so this is
    // less an assertion about behaviour than a statement that the type cannot
    // express the thing criterion 12 forbids.
    const auto &beforeSequence = std::get<SequencePreviewStatus>(before.context);
    const auto &afterSequence = std::get<SequencePreviewStatus>(after.context);
    return require(after.generation == before.generation
                       && after.statusSeq == before.statusSeq
                       && after.phase == before.phase
                       && afterSequence.timelineFrame == beforeSequence.timelineFrame,
                   "A renderer acknowledgement must not advance transport: not the "
                   "playhead, not the generation, not the status sequence.");
}

bool verifyPreviewDriverSchedulesAudioIndependently()
{
    using namespace mini_editor::playback_core;

    DriverHarness harness;

    // Before A1 starts: video only, and the lane is explicitly silent rather
    // than left in whatever state it happened to be in.
    const PreviewDriveOutcome beforeAudio = harness.seekTo(0);
    if (!require(beforeAudio.openClip && beforeAudio.openClip->clipId == 10
                     && !beforeAudio.openAudioClip && beforeAudio.silenceAudio,
                 "Before A1 starts, the audio lane must be silenced explicitly."))
        return false;

    // A1 opens on its own boundary, which is nowhere near V1's.
    const PreviewDriveOutcome audioStart = harness.seekTo(15);
    if (!require(audioStart.openAudioClip && audioStart.openAudioClip->clipId == 20
                     && audioStart.openAudioClip->immutableSourceLocator == "voice.wav"
                     && !audioStart.openClip && !audioStart.silenceAudio,
                 "A1 must open at its own start frame without re-opening V1."))
        return false;

    // The fade ramp, from the same policy the legacy path uses for opacity:
    // 60 frames with a 10-frame ramp at each end.
    if (!require(audioStart.audioLevelPercent == 0
                     && harness.seekTo(20).audioLevelPercent == 50
                     && harness.seekTo(25).audioLevelPercent == 100
                     && harness.seekTo(70).audioLevelPercent == 50,
                 "The A1 level must follow the clip's fade ramp at each frame."))
        return false;

    // The heart of decision C: V1's gap, and then V1's next clip boundary,
    // must leave A1 completely alone. One player per lane is what makes this
    // possible -- with a single player, re-opening V1 would cut the audio.
    const PreviewDriveOutcome videoGap = harness.seekTo(40);
    if (!require(videoGap.showNothing && !videoGap.silenceAudio
                     && !videoGap.openAudioClip
                     && harness.driver.openAudioClipId() == 20,
                 "A gap on V1 must not silence A1, and must not re-open it either."))
        return false;

    const PreviewDriveOutcome videoBoundary = harness.seekTo(60);
    if (!require(videoBoundary.openClip && videoBoundary.openClip->clipId == 11
                     && !videoBoundary.openAudioClip && !videoBoundary.silenceAudio,
                 "Crossing a V1 clip boundary must not interrupt A1."))
        return false;

    // And the reverse: A1 ending must not disturb V1.
    const PreviewDriveOutcome audioEnd = harness.seekTo(80);
    if (!require(audioEnd.silenceAudio && !harness.driver.openAudioClipId()
                     && !audioEnd.showNothing
                     && harness.driver.openClipId() == 11,
                 "A1 ending must silence only the audio lane, leaving V1 open."))
        return false;

    // Mix state travels with the snapshot rather than with the transport.
    DriverHarness muted(MediaAvailability::Available);
    muted.driver.installSnapshot(
        makeDriverFixture(muted.sequenceId, MediaAvailability::Available,
                          /*isVideoTrackMuted=*/true));
    muted.session.applyCommand(
        InstallSnapshot{makeDriverFixture(muted.sequenceId, MediaAvailability::Available, true)},
        PlaybackCommandId::create());
    return require(muted.seekTo(20).isVideoTrackMuted,
                   "The snapshot's isVideoTrackMuted must reach the adapter.");
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
        || !verifyPlaybackEngineFailureObservation()
        || !verifySnapshotTimelineResolverBoundaries()
        || !verifySnapshotTimelineResolverSourceTime()
        || !verifySnapshotTimelineResolverEdgeCases()
        || !verifySnapshotInstallRevisionRules()
        || !verifySessionRetargetAcrossProjectReload()
        || !verifyPreviewDriverFollowsThePlayheadAcrossClips()
        || !verifyPreviewDriverBoundsScrubbingAndSkipsStills()
        || !verifyPreviewDriverHoldsWhenItCannotResolve()
        || !verifyTimelineTransportView()
        || !verifyFramePresentedIsDiagnosticsOnly()
        || !verifyPreviewDriverSchedulesAudioIndependently()) {
        return 1;
    }

    return 0;
}
