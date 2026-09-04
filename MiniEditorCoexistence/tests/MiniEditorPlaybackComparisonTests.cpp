// M5-07 -- the automated gate for flipping the default (Decision D).
//
// Both transports are driven through one script and their observable traces
// are diffed. The point is to make the equivalence bar checkable rather than
// argued: if this matrix is not green, the default does not flip.
//
// Determinism comes from removing wall-clock time from both sides. The legacy
// path advances exactly one frame per advanceOneFrame() tick. The new path
// derives its position from an IPlaybackClock, so the harness injects a fake
// clock and steps it by exactly one frame-time per tick -- scaled by the
// transport rate, because that is how the legacy tick interval encodes rate
// (a faster rate shortens the interval; each tick still advances one frame).
//
// What is compared is *transport*: the ordered (phase, timelineFrame) samples
// and the terminal state. Content resolution -- which clip, which source
// time, which fade -- is covered by the M5-01/M5-02/M5-06 unit tests. The
// content variations here (still, gap, trimmed source-in, clip boundary)
// exist to prove that content does not perturb transport: a gap must not
// stall the playhead, a still must not stop it.
//
// Deliberately not compared: rendered pixels (Decision F). Visual and audio
// fidelity is confirmed by human validation at M5-08.

#include "EditorProject.h"
#include "EditorSession.h"
#include "MediaLibrary.h"
#include "PlaybackBackend.h"
#include "ProjectState.h"
#include "SequencePlaybackSnapshotBuilder.h"
#include "TimelineModel.h"
#include "playback_core/PlaybackClock.h"
#include "playback_core/PlaybackSession.h"
#include "playback_core/PreviewPresentation.h"
#include "playback_core/SequencePreviewDriver.h"
#include "playback_core/TimelineTransportView.h"
#include "playback_core/VideoWorkScheduler.h"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace mini_editor::playback_core;

namespace {

constexpr int kFrameRateNumerator = 30;
constexpr int kSequenceDurationFrames = 90;

// ---------------------------------------------------------------- test doubles

class FakeClock final : public IPlaybackClock {
public:
    MasterClockTime now() const override
    {
        return MasterClockTime::fromMicroseconds(microseconds_);
    }

    void advance(std::int64_t microseconds) { microseconds_ += microseconds; }

private:
    std::int64_t microseconds_ = 0;
};

class FakeDecodeService final : public IVideoDecodeService {
public:
    void requestDecode(VideoDecodeRequest, std::function<void(DecodedVideoFrame)>) override {}
};

class FakeCompositor final : public IVideoCompositor {
public:
    void composite(DecodedVideoFrame, FramePresentationRequest,
                   std::function<void(CompositedVideoFrame)>) override {}
};

// ------------------------------------------------------------------- the trace

// Three transport states both paths can express. Seeking and Prerolling are
// not among them: they resolve inside a single applyCommand() in this
// milestone, so no observer ever sees one (see TimelineTransportView).
enum class Phase { Stopped, Paused, Playing };

const char *phaseName(Phase phase)
{
    switch (phase) {
    case Phase::Stopped: return "stopped";
    case Phase::Paused:  return "paused";
    case Phase::Playing: return "playing";
    }
    return "?";
}

struct Sample final {
    Phase phase;
    int timelineFrame;
};

bool operator==(const Sample &left, const Sample &right)
{
    return left.phase == right.phase && left.timelineFrame == right.timelineFrame;
}

std::string describe(const Sample &sample)
{
    std::ostringstream text;
    text << phaseName(sample.phase) << '@' << sample.timelineFrame;
    return text.str();
}

// -------------------------------------------------------------------- the script

struct Step final {
    enum class Kind {
        Toggle,          // the transport button / space bar
        Stop,
        StepForward,
        StepBackward,
        SeekToFrame,     // clicking the timeline ruler
        SetRatePercent,
        Tick,            // one legacy timer tick / one frame-time of clock
        ContentEdit,     // a playback-affecting edit: new duration, same sequence
        ReloadProject,   // a new project: new sequence identity
        SelectClip       // an editing selection, not a transport command
    };

    Kind kind;
    int value = 0;
};

Step toggle()                 { return { Step::Kind::Toggle }; }
Step stop()                   { return { Step::Kind::Stop }; }
Step stepForward()            { return { Step::Kind::StepForward }; }
Step stepBackward()           { return { Step::Kind::StepBackward }; }
Step seekTo(int frame)        { return { Step::Kind::SeekToFrame, frame }; }
Step setRate(int percent)     { return { Step::Kind::SetRatePercent, percent }; }
Step tick(int count = 1)      { return { Step::Kind::Tick, count }; }
Step contentEdit(int frames)  { return { Step::Kind::ContentEdit, frames }; }
Step reloadProject()          { return { Step::Kind::ReloadProject }; }
Step selectClip()             { return { Step::Kind::SelectClip }; }

// ------------------------------------------------------------- content fixtures

// V1 has a trimmed video clip, a gap, and a still; A1 spans both. Which of
// those a scenario exercises is decided by where its script puts the
// playhead, not by using a different project -- one fixture keeps the two
// paths comparing the same thing throughout.
//
//  frame   0        30       60       90
//  V1     [clip 1  ][  gap  ][ still ]
//  A1     [------- clip 3 -----------]
EditorProject makeComparisonProject(int videoClipDurationFrames = 30)
{
    EditorProject project = EditorProject::createDefault(0);
    project.mediaAssets.clear();
    project.timelineItems.clear();
    project.clipSettings.clear();
    project.timelineClips.clear();

    project.mediaAssets.push_back({ 1, "video.mp4", L"Video", MediaKind::Video, 120, 0x336699 });
    project.mediaAssets.push_back({ 2, "still.png", L"Still", MediaKind::Image, 30, 0x996633 });
    project.mediaAssets.push_back({ 3, "voice.wav", L"Voice", MediaKind::Audio, 120, 0x669933 });

    // sourceInFrame 10 is the trimmed-source-in case (scenario 10).
    project.timelineItems.push_back(
        { 1, 1, TimelineTrackType::Video, { 0, videoClipDurationFrames, 10 }, {} });
    project.timelineItems.push_back(
        { 2, 2, TimelineTrackType::Video, { 60, 30, 0 }, {} });
    project.timelineItems.push_back(
        { 3, 3, TimelineTrackType::Audio, { 0, 90, 0 }, {} });

    project.clipSettings.push_back({});
    project.timelineClips.push_back({ 0, 30, 0 });
    return project;
}

int contentDurationFrames(const EditorProject &project)
{
    int greatestEnd = 0;
    for (const TimelineClip &clip : project.timelineItems)
        greatestEnd = std::max(greatestEnd, clip.state.startFrame + clip.state.durationFrames);
    return greatestEnd;
}

// ----------------------------------------------------------------- legacy path

// EditorSession plus SimulatedPlaybackBackend, driven exactly as
// EditorCommandController and MainFrame::OnTimer drive them.
class LegacyTransport final {
public:
    LegacyTransport()
        : session_(0)
        , backend_(session_)
    {
        project_ = makeComparisonProject();
        session_.replaceProject(project_);
        session_.selectTimelineClip(1);
        session_.setPlaybackDuration(contentDurationFrames(project_), true);
        mutationsBeforeScript_ = session_.legacyTimelinePlaybackMutationCount();
    }

    void apply(const Step &step)
    {
        switch (step.kind) {
        case Step::Kind::Toggle:
            backend_.executeCommand(LegacyPlaybackCommand::TogglePlayPause);
            break;
        case Step::Kind::Stop:
            backend_.executeCommand(LegacyPlaybackCommand::Stop);
            break;
        case Step::Kind::StepForward:
            backend_.executeCommand(LegacyPlaybackCommand::StepForward);
            break;
        case Step::Kind::StepBackward:
            backend_.executeCommand(LegacyPlaybackCommand::StepBackward);
            break;
        case Step::Kind::SeekToFrame:
            session_.seekTimeline(step.value);
            break;
        case Step::Kind::SetRatePercent:
            session_.updatePlaybackRatePercent(step.value);
            break;
        case Step::Kind::Tick:
            for (int index = 0; index < std::max(1, step.value); ++index)
                backend_.advanceOneFrame();
            break;
        case Step::Kind::ContentEdit:
            project_ = makeComparisonProject(step.value);
            session_.replaceProject(project_);
            session_.selectTimelineClip(1);
            session_.setPlaybackDuration(contentDurationFrames(project_), false);
            break;
        case Step::Kind::ReloadProject:
            project_ = makeComparisonProject();
            session_.replaceProject(project_);
            session_.selectTimelineClip(1);
            session_.setPlaybackDuration(contentDurationFrames(project_), true);
            break;
        case Step::Kind::SelectClip:
            session_.selectTimelineClip(2);
            break;
        }
    }

    Sample sample() const
    {
        const PlaybackState &state = session_.timelinePlaybackState();
        const Phase phase = state.isPlaying ? Phase::Playing
                          : state.isPaused  ? Phase::Paused
                                            : Phase::Stopped;
        return { phase, state.currentFrame };
    }

    std::size_t timelineMutationsDuringScript() const
    {
        return session_.legacyTimelinePlaybackMutationCount() - mutationsBeforeScript_;
    }

private:
    EditorProject project_;
    EditorSession session_;
    SimulatedPlaybackBackend backend_;
    std::size_t mutationsBeforeScript_ = 0;
};

// -------------------------------------------------------------- new engine path

// PlaybackSession is the whole transport. The presentation stack
// (SequencePreviewDriver, the coordinator, the scheduler) is driven alongside
// it on purpose: if presentation could feed back into transport, this is
// where it would show, and the mutation counter below would catch it doing so
// through EditorSession.
class EngineTransport final {
public:
    EngineTransport()
        : session_(0)
        , scheduler_(decoder_, compositor_, [](CompositedVideoFrame) {})
    {
        project_ = makeComparisonProject();
        session_.replaceProject(project_);
        session_.selectTimelineClip(1);
        mutationsBeforeScript_ = session_.legacyTimelinePlaybackMutationCount();

        playback_ = std::make_unique<PlaybackSession>(
            PlaybackSource{SequencePreview{*session_.projectRuntime().activeSequenceId()}},
            clock_);
        driver_ = std::make_unique<SequencePreviewDriver>(coordinator_, scheduler_, clock_);
        installCurrentProject();
        publish(/*transportJustRepositioned=*/true);
    }

    void apply(const Step &step)
    {
        switch (step.kind) {
        case Step::Kind::Toggle:
            if (currentPhase() == PlaybackPhase::Playing)
                submit(Pause{});
            else
                submit(Play{});
            break;
        case Step::Kind::Stop:
            submit(Stop{});
            break;
        case Step::Kind::StepForward:
            submitStep(1);
            break;
        case Step::Kind::StepBackward:
            submitStep(-1);
            break;
        case Step::Kind::SeekToFrame:
            submitSeekToFrame(step.value);
            break;
        case Step::Kind::SetRatePercent:
            submit(SetRate{step.value});
            break;
        case Step::Kind::Tick:
            for (int index = 0; index < std::max(1, step.value); ++index) {
                // One frame of sequence time, converted to clock time at the
                // current rate -- the clock-domain equivalent of the legacy
                // timer's rate-scaled interval.
                clock_.advance(clockElapsedFor(
                    SequenceDuration::fromMicroseconds(
                        sequenceTimeAtFrameStart(TimelineFrame::fromFrameNumber(1),
                                                 frameRate()).microsecondsForAdapter()),
                    ratePercent_).microsecondsForClockAdapter());
                // What PlaybackEngine does for a queued clock observation:
                // a free-running transport applies no commands, so the end of
                // the sequence is only noticed because somebody looks.
                playback_->observeClock();
                publish(/*transportJustRepositioned=*/false);
            }
            break;
        case Step::Kind::ContentEdit:
            project_ = makeComparisonProject(step.value);
            session_.replaceProject(project_);
            session_.selectTimelineClip(1);
            installCurrentProject();
            break;
        case Step::Kind::ReloadProject:
            project_ = makeComparisonProject();
            session_.replaceProject(project_);
            session_.selectTimelineClip(1);
            installCurrentProject();
            break;
        case Step::Kind::SelectClip:
            // An editing selection is not a transport command. The routed
            // path submits nothing at all, which is the assertion.
            session_.selectTimelineClip(2);
            break;
        }
    }

    Sample sample() const
    {
        const std::optional<TimelineTransportView> view =
            timelineTransportViewFor(playback_->status());
        if (!view)
            return { Phase::Stopped, 0 };

        const Phase phase = view->isPlaying ? Phase::Playing
                          : view->isPaused  ? Phase::Paused
                                            : Phase::Stopped;
        return { phase, static_cast<int>(view->timelineFrame) };
    }

    std::size_t timelineMutationsDuringScript() const
    {
        return session_.legacyTimelinePlaybackMutationCount() - mutationsBeforeScript_;
    }

private:
    FrameRate frameRate() const { return FrameRate(kFrameRateNumerator, 1); }
    PlaybackPhase currentPhase() const { return playback_->status().phase; }

    void submit(const PlaybackCommand &command)
    {
        playback_->applyCommand(command, PlaybackCommandId::create());
        if (const auto *rate = std::get_if<SetRate>(&command))
            ratePercent_ = rate->ratePercent;
        publish(/*transportJustRepositioned=*/true);
    }

    void submitSeekToFrame(int frame)
    {
        submit(Seek{sequenceTimeAtFrameStart(
            TimelineFrame::fromFrameNumber(std::max(0, frame)), frameRate())});
    }

    void submitStep(int delta)
    {
        // What TimelineEngineRouter::applyIntent does for the two step
        // intents: clamp into the sequence and seek there.
        const auto &context = std::get<SequencePreviewStatus>(playback_->status().context);
        const std::int64_t lastFrame = std::max<std::int64_t>(
            0, context.sequenceDuration.frames() - 1);
        submitSeekToFrame(static_cast<int>(std::clamp<std::int64_t>(
            context.timelineFrame.frameNumber() + delta, 0, lastFrame)));
    }

    void installCurrentProject()
    {
        EditorProject project = project_;
        project.mediaAssets = project_.mediaAssets;
        const SnapshotBuildResult result =
            SequencePlaybackSnapshotBuilder::build(project, session_.projectRuntime());
        const auto *snapshot = std::get_if<SequencePlaybackSnapshotPtr>(&result);
        if (snapshot == nullptr) {
            std::cerr << "harness: snapshot build failed: "
                      << std::get<SnapshotBuildError>(result).message << '\n';
            return;
        }
        playback_->applyCommand(InstallSnapshot{*snapshot}, PlaybackCommandId::create());
        driver_->installSnapshot(*snapshot);
        publish(/*transportJustRepositioned=*/true);
    }

    void publish(bool transportJustRepositioned)
    {
        driver_->notifyPlaybackStatus(playback_->status(), transportJustRepositioned);
    }

    EditorProject project_;
    EditorSession session_;
    FakeClock clock_;
    PreviewPresentationCoordinator coordinator_;
    FakeDecodeService decoder_;
    FakeCompositor compositor_;
    VideoWorkScheduler scheduler_;
    std::unique_ptr<PlaybackSession> playback_;
    std::unique_ptr<SequencePreviewDriver> driver_;
    int ratePercent_ = 100;
    std::size_t mutationsBeforeScript_ = 0;
};

// ------------------------------------------------------------------ the compare

struct ScenarioResult final {
    std::string name;
    bool passed = false;
    // Every differing sample, not just the first: one blocking difference
    // early in a script would otherwise hide every later one, and a gate that
    // reveals its failures one run at a time is a slow gate.
    std::vector<std::string> differences;
    std::size_t engineTimelineMutations = 0;
};

ScenarioResult run(const std::string &name, const std::vector<Step> &script)
{
    LegacyTransport legacy;
    EngineTransport engine;

    ScenarioResult result;
    result.name = name;

    std::vector<Sample> legacyTrace{legacy.sample()};
    std::vector<Sample> engineTrace{engine.sample()};
    for (const Step &step : script) {
        legacy.apply(step);
        engine.apply(step);
        legacyTrace.push_back(legacy.sample());
        engineTrace.push_back(engine.sample());
    }

    result.engineTimelineMutations = engine.timelineMutationsDuringScript();

    for (std::size_t index = 0; index < legacyTrace.size(); ++index) {
        if (legacyTrace[index] == engineTrace[index])
            continue;

        std::ostringstream difference;
        difference << "sample " << index << ": legacy " << describe(legacyTrace[index])
                   << " vs engine " << describe(engineTrace[index]);
        result.differences.push_back(difference.str());
    }

    if (result.engineTimelineMutations != 0) {
        result.differences.push_back(
            "the engine path called " + std::to_string(result.engineTimelineMutations)
            + " legacy timeline playback mutator(s)");
    }

    result.passed = result.differences.empty();
    return result;
}

std::vector<ScenarioResult> runMatrix()
{
    std::vector<ScenarioResult> results;

    results.push_back(run("1  Play from stopped",
        { toggle(), tick(5) }));

    results.push_back(run("2  Pause, then resume",
        { toggle(), tick(5), toggle(), tick(3), toggle(), tick(4) }));

    results.push_back(run("3  Seek while playing",
        { toggle(), tick(5), seekTo(40), tick(3) }));

    results.push_back(run("4  Seek while paused",
        { toggle(), tick(5), toggle(), seekTo(12), tick(2) }));

    results.push_back(run("5  Seek while stopped",
        { seekTo(20), tick(2) }));

    results.push_back(run("6  Step at both boundaries",
        { stepBackward(), stepBackward(), stepForward(), stepForward(),
          seekTo(kSequenceDurationFrames - 1), stepForward(), stepForward(),
          stepBackward() }));

    results.push_back(run("7  Clip-boundary crossing while playing",
        { seekTo(28), toggle(), tick(6) }));

    results.push_back(run("8  Still image on V1",
        { seekTo(62), toggle(), tick(6) }));

    results.push_back(run("9  Gap on V1",
        { seekTo(35), toggle(), tick(6) }));

    results.push_back(run("10 Trimmed source-in",
        { seekTo(0), toggle(), tick(10) }));

    results.push_back(run("11 End of timeline, then play again",
        { seekTo(kSequenceDurationFrames - 3), toggle(), tick(6), toggle(), tick(3) }));

    results.push_back(run("12 Rate changes",
        { setRate(50), toggle(), tick(4), setRate(100), tick(4),
          setRate(150), tick(4), setRate(200), tick(4) }));

    results.push_back(run("13 Edit while playing",
        { toggle(), tick(4), contentEdit(45), tick(3) }));

    results.push_back(run("14 Undo/redo while paused",
        { toggle(), tick(4), toggle(), contentEdit(45), contentEdit(30), tick(2) }));

    results.push_back(run("15 Project reload",
        { toggle(), tick(4), reloadProject(), tick(3), toggle(), tick(2) }));

    results.push_back(run("16 Select a clip while paused",
        { toggle(), tick(4), toggle(), selectClip(), selectClip() }));

    return results;
}

} // namespace

int main()
{
    const std::vector<ScenarioResult> results = runMatrix();

    std::size_t failures = 0;
    std::cout << "\nM5-07 legacy/engine transport comparison"
              << " (zero-frame tolerance)\n"
              << std::string(72, '-') << '\n';
    for (const ScenarioResult &result : results) {
        std::cout << (result.passed ? "  PASS  " : "  FAIL  ")
                  << std::left << std::setw(44) << result.name << '\n';
        if (result.passed)
            continue;

        ++failures;
        // A long run of the same difference says one thing, not twenty.
        std::size_t shown = 0;
        for (const std::string &difference : result.differences) {
            if (shown++ == 6) {
                std::cout << "          ... and " << (result.differences.size() - 6)
                          << " more\n";
                break;
            }
            std::cout << "          " << difference << '\n';
        }
    }
    std::cout << std::string(72, '-') << '\n'
              << (results.size() - failures) << " of " << results.size()
              << " scenarios match\n\n";

    return failures == 0 ? 0 : 1;
}
