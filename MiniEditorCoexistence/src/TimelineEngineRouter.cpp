#include "TimelineEngineRouter.h"

#include <QVideoWidget>

#include <Windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>

using namespace mini_editor::playback_core;

namespace {

void logLine(const wchar_t *format, ...)
{
    wchar_t buffer[512];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
    va_end(args);
    ::OutputDebugStringW(L"[TimelineEngineRouter] ");
    ::OutputDebugStringW(buffer);
    ::OutputDebugStringW(L"\n");
}

} // namespace

TimelineEngineRouter::TimelineEngineRouter(HWND notifyTarget, UINT notifyMessage,
                                           SequenceId sequenceId,
                                           QVideoSink *engineSurfaceSink)
    : bridge_(notifyTarget, notifyMessage)
    , engine_(std::make_unique<PlaybackEngine>(
          PlaybackSource{SequencePreview{sequenceId}}, clock_, &bridge_))
{
    if (engineSurfaceSink != nullptr) {
        // M5-05: the panel's own engine surface. The legacy sink is not
        // touched, so source preview keeps rendering exactly as before.
        worker_.attachExternalVideoOutput(engineSurfaceSink);
    } else {
        // No panel surface supplied: keep M4-06's standalone window so the
        // manual smoke-test configuration still shows something.
        previewWindow_ = std::make_unique<QVideoWidget>();
        previewWindow_->setWindowTitle(QStringLiteral("Timeline Preview (New Engine)"));
        previewWindow_->resize(640, 360);
        worker_.attachExternalVideoOutput(previewWindow_->videoSink());
    }

    // M5-02, decision A: the coordinator and the scheduler stop being tested
    // machinery nothing calls and become the production routed path.
    scheduler_ = std::make_unique<VideoWorkScheduler>(
        worker_.videoDecodeService(), worker_.videoCompositor(),
        [this](CompositedVideoFrame frame) { onFrameComposited(std::move(frame)); });
    driver_ = std::make_unique<SequencePreviewDriver>(coordinator_, *scheduler_, clock_);

    // 15 ms: half a frame at 30 fps, so a clip boundary is noticed within the
    // frame it happens in rather than after it.
    presentationTimer_.setInterval(15);
    presentationTimer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&presentationTimer_, &QTimer::timeout, this,
                     &TimelineEngineRouter::onPresentationTick);

    // M4-08: the worker never touches PlaybackSession/PlaybackEngine directly
    // -- it only emits this signal. Reporting the failure through the
    // engine's serialized queue, tagged with whatever session/generation is
    // current right now, is this handler's job, exactly like
    // EngineSmokeTestSession's identical connection (src/EngineSmokeTestSession.cpp).
    // A stale identity by the time this is processed is discarded by
    // PlaybackSession itself.
    QObject::connect(&worker_, &QtPlaybackMediaWorker::mediaErrorOccurred, this,
                     [this](const QString &message) {
                         logLine(L"QMediaPlayer error: %hs", message.toStdString().c_str());
                         reportWorkerFailure(*engine_, message.toStdString());
                     });

    logLine(L"Router constructed. sessionId=%llu",
            static_cast<unsigned long long>(engine_->status().sessionId.valueForDiagnostics()));
}

TimelineEngineRouter::~TimelineEngineRouter()
{
    logLine(L"Router shutting down.");
    presentationTimer_.stop();
    coordinator_.shutdown();
    if (engine_)
        engine_->shutdownAndJoin();
}

void TimelineEngineRouter::installSnapshot(SequencePlaybackSnapshotPtr snapshot)
{
    if (!snapshot) {
        logLine(L"installSnapshot() called with a null snapshot; ignoring.");
        return;
    }

    currentFrameRate_ = snapshot->frameRate;
    currentDuration_ = snapshot->duration;

    logLine(L"Installing snapshot: sequenceId=%llu revision=%llu videoClips=%zu audioClips=%zu "
            L"mediaDescriptors=%zu durationFrames=%lld",
            static_cast<unsigned long long>(snapshot->sequenceId.valueForDiagnostics()),
            static_cast<unsigned long long>(snapshot->revision.value()),
            snapshot->videoClips.size(), snapshot->audioClips.size(), snapshot->media.size(),
            static_cast<long long>(snapshot->duration.frames()));

    // M5-03: the session refuses a revision that is not strictly newer for a
    // sequence it already has, and submit() cannot tell us so -- it queues the
    // command and the refusal happens later on the engine thread. Ask the
    // shared predicate here instead, so a view-only editor change that
    // rebuilds an identical snapshot does not re-open media below.
    if (lastInstalledSnapshot_
        && !snapshotSupersedes(lastInstalledSnapshot_->sequenceId,
                               lastInstalledSnapshot_->revision, *snapshot)) {
        logLine(L"Snapshot does not supersede the installed one (same sequence, revision "
                L"%llu is not newer than %llu); leaving the current preview in place.",
                static_cast<unsigned long long>(snapshot->revision.value()),
                static_cast<unsigned long long>(lastInstalledSnapshot_->revision.value()));
        return;
    }
    lastInstalledSnapshot_ = snapshot;

    engine_->submit(InstallSnapshot{snapshot}, PlaybackCommandId::create());

    // Which clip to open, and when, is now decided per status by the driver
    // against the playhead -- not once, here, from clip zero.
    driver_->installSnapshot(snapshot);
}

void TimelineEngineRouter::applyIntent(EditorIntent intent)
{
    switch (intent) {
    case EditorIntent::TogglePlayback: {
        const PlaybackPhase phase = engine_->status().phase;
        if (phase == PlaybackPhase::Playing)
            engine_->submit(Pause{}, PlaybackCommandId::create());
        else
            engine_->submit(Play{}, PlaybackCommandId::create());
        break;
    }
    case EditorIntent::StopPlayback:
        engine_->submit(Stop{}, PlaybackCommandId::create());
        break;
    case EditorIntent::StepBackward:
    case EditorIntent::StepForward: {
        const PlaybackStatus status = engine_->status();
        const auto *sequence = std::get_if<SequencePreviewStatus>(&status.context);
        if (!sequence)
            break;
        const std::int64_t delta = (intent == EditorIntent::StepForward) ? 1 : -1;
        const std::int64_t maxFrame = std::max<std::int64_t>(0, currentDuration_.frames() - 1);
        const std::int64_t newFrameNumber = std::clamp<std::int64_t>(
            sequence->timelineFrame.frameNumber() + delta, 0, maxFrame);
        const SequenceTime target =
            sequenceTimeAtFrameStart(TimelineFrame::fromFrameNumber(newFrameNumber), currentFrameRate_);
        engine_->submit(Seek{target}, PlaybackCommandId::create());
        break;
    }
    default:
        break; // Not a playback intent; MainFrame only routes the four above here.
    }
}

void TimelineEngineRouter::seekToTimelineFrame(int timelineFrame)
{
    const TimelineFrame frame = TimelineFrame::fromFrameNumber(std::max(0, timelineFrame));
    const SequenceTime target = sequenceTimeAtFrameStart(frame, currentFrameRate_);
    engine_->submit(Seek{target}, PlaybackCommandId::create());
}

void TimelineEngineRouter::setTransportViewSink(TransportViewSink sink)
{
    transportViewSink_ = std::move(sink);
}

void TimelineEngineRouter::setEnginePresentationActiveSink(EnginePresentationActiveSink sink)
{
    enginePresentationActiveSink_ = std::move(sink);
}

const PresentationDiagnostics &TimelineEngineRouter::presentationDiagnostics() const
{
    return diagnostics_;
}

void TimelineEngineRouter::onEngineFrameCommitted()
{
    // ADR-003 criterion 12: this is presentation diagnostics and nothing
    // else. There is no path from here to the clock, the session, or the
    // command queue -- FramePresented carries no means of moving anything.
    const std::optional<FramePresentationRequest> request = coordinator_.currentRequest();
    if (!request)
        return;

    diagnostics_.recordPresented(FramePresented{
        request->presentationSessionId, request->requestId, request->authority,
        presentedPositionFor(request->target)});
}

void TimelineEngineRouter::onNotification()
{
    handleEvents(bridge_.drain());
}

void TimelineEngineRouter::handleEvents(std::vector<PlaybackEvent> events)
{
    for (const PlaybackEvent &event : events) {
        const auto *status = std::get_if<PlaybackStatus>(&event);
        if (!status)
            continue;

        // Resolve and open before mirroring the phase, so that on the first
        // Play the clip's media is already on its way to the worker thread
        // when play() is queued behind it.
        //
        // Every status here is the result of a command this router submitted,
        // and the routed path issues no editing selections yet, so
        // "repositioned" is true for all of them. M5-05 introduces
        // timeline-selection overrides and will have to distinguish Pause and
        // SetRate from the override-clearing commands.
        drivePreview(*status, /*transportJustRepositioned=*/true);

        if (status->phase != lastAppliedPhase_) {
            lastAppliedPhase_ = status->phase;
            switch (status->phase) {
            case PlaybackPhase::Playing:
                worker_.play();
                presentationTimer_.start();
                break;
            case PlaybackPhase::Paused:
            case PlaybackPhase::Stopped:
                presentationTimer_.stop();
                worker_.pause();
                break;
            case PlaybackPhase::Failed:
                presentationTimer_.stop();
                logLine(L"phase=Failed error=%hs",
                        status->error ? status->error->message.c_str() : "(none)");
                break;
            default:
                break;
            }
        }
    }
}

void TimelineEngineRouter::onPresentationTick()
{
    // A plain refresh, not a reposition: the transport is simply where the
    // clock says it is.
    drivePreview(engine_->status(), /*transportJustRepositioned=*/false);
}

void TimelineEngineRouter::drivePreview(const PlaybackStatus &status,
                                        bool transportJustRepositioned)
{
    const PreviewDriveOutcome outcome =
        driver_->notifyPlaybackStatus(status, transportJustRepositioned);

    // ADR-002: the legacy timeline PlaybackState survives as a painting cache
    // populated from a status publication. This is that publication, and it
    // goes one way only -- nothing here ever reads the cache back.
    if (transportViewSink_) {
        if (const auto view = timelineTransportViewFor(status))
            transportViewSink_(*view);
    }

    // Both lanes follow one transport rate. Nothing submits SetRate on the
    // routed path yet, but if anything ever does, the two players desyncing
    // is not a failure that would announce itself.
    if (lastRatePercent_ != status.ratePercent) {
        lastRatePercent_ = status.ratePercent;
        worker_.setRatePercent(status.ratePercent);
        worker_.setAudioRatePercent(status.ratePercent);
    }

    driveAudioLane(outcome, status.phase);

    // The panel paints engine frames only while a clip is actually open, so a
    // gap falls back to the panel's own "no media at this position" rendering
    // instead of leaving the previous clip's last frame up.
    setEnginePresentationActive(driver_->openClipId().has_value());

    if (outcome.showNothing) {
        // A gap, the tail past the last clip, or missing media. Freeze rather
        // than tear down: the engine's clock keeps running through a gap, and
        // the next clip will open on its own when the playhead reaches it.
        worker_.pause();
        return;
    }
    if (!outcome.openClip)
        return;

    const PreviewSourceChange &change = *outcome.openClip;
    if (change.mediaKind != PlaybackMediaKind::Video) {
        // A still (or an audio-only clip) has no source timeline. Handing one
        // to QMediaPlayer is how a picture becomes a decoder error and, from
        // there, a Failed session. Rendering stills belongs to the dedicated
        // presentation path in M5-05.
        logLine(L"Clip %d is not video (mediaKind=%d); not opening it in the continuous "
                L"player. Still-image presentation arrives with M5-05.",
                change.clipId, static_cast<int>(change.mediaKind));
        worker_.pause();
        return;
    }

    logLine(L"Playhead entered clip %d; opening %hs", change.clipId,
            change.immutableSourceLocator.c_str());
    worker_.openSource(QString::fromStdString(change.immutableSourceLocator));
    if (change.sourceTime)
        worker_.seekTo(*change.sourceTime);
    if (status.phase == PlaybackPhase::Playing)
        worker_.play();

    if (!previewWindow_->isVisible())
        previewWindow_->show();
}

void TimelineEngineRouter::driveAudioLane(const PreviewDriveOutcome &outcome,
                                          PlaybackPhase phase)
{
    // The snapshot's mix state, applied to the *video* player's own audio.
    // Sent only on change: every drive would otherwise re-issue it at the
    // presentation tick's cadence.
    const int videoTrackMuted = outcome.isVideoTrackMuted ? 1 : 0;
    if (lastVideoTrackMuted_ != videoTrackMuted) {
        lastVideoTrackMuted_ = videoTrackMuted;
        worker_.setVideoTrackAudioMuted(outcome.isVideoTrackMuted);
    }

    if (outcome.silenceAudio) {
        // No A1 clip here. Silence the lane rather than tearing it down: the
        // next clip is usually a few frames away, and re-opening a file for
        // every gap is how a timeline starts stuttering.
        if (!isAudioSilenced_) {
            isAudioSilenced_ = true;
            worker_.setAudioMuted(true);
            worker_.pauseAudio();
        }
        return;
    }

    if (outcome.openAudioClip) {
        logLine(L"Playhead entered A1 clip %d; opening %hs",
                outcome.openAudioClip->clipId,
                outcome.openAudioClip->immutableSourceLocator.c_str());
        worker_.openAudioSource(
            QString::fromStdString(outcome.openAudioClip->immutableSourceLocator));
        if (outcome.openAudioClip->sourceTime)
            worker_.seekAudioTo(*outcome.openAudioClip->sourceTime);
    }

    if (lastAudioLevelPercent_ != outcome.audioLevelPercent) {
        lastAudioLevelPercent_ = outcome.audioLevelPercent;
        worker_.setAudioLevelPercent(outcome.audioLevelPercent);
    }

    // Seeking while stopped or paused prepares the decoder position but must
    // never leak a short packet through the speakers -- the same rule the
    // legacy path follows.
    const bool shouldSound = phase == PlaybackPhase::Playing;
    if (isAudioSilenced_ != !shouldSound || outcome.openAudioClip) {
        isAudioSilenced_ = !shouldSound;
        worker_.setAudioMuted(!shouldSound);
        if (shouldSound)
            worker_.playAudio();
        else
            worker_.pauseAudio();
    }
}

void TimelineEngineRouter::setEnginePresentationActive(bool active)
{
    if (isEnginePresentationActive_ == active)
        return;

    isEnginePresentationActive_ = active;
    if (enginePresentationActiveSink_)
        enginePresentationActiveSink_(active);
}

void TimelineEngineRouter::onFrameComposited(CompositedVideoFrame frame)
{
    // The scheduler calls back on whichever thread the compositor finished
    // on. PreviewPresentationCoordinator belongs to the GUI thread, so the
    // currency check has to happen there, not here (ADR-005).
    const PresentationSessionId presentationSessionId = frame.presentationSessionId;
    const PresentationRequestId requestId = frame.requestId;
    const PresentationAuthority authority = frame.authority;
    QMetaObject::invokeMethod(this,
        [this, presentationSessionId, requestId, authority, accepted = std::move(frame)] {
            // ADR-003: the worker does not decide its own currency. A frame
            // that finished after a newer want superseded it is dropped here.
            if (!coordinator_.isCurrentRequest(presentationSessionId, requestId, authority)) {
                logLine(L"Discarding a composited frame: request %llu is no longer current.",
                        static_cast<unsigned long long>(requestId.value()));
                return;
            }
            // Readiness, not presentation. The pixels reach the panel's engine
            // sink on their own; the panel acknowledges the commit separately
            // through onEngineFrameCommitted(), which is what keeps the two
            // countable apart (ADR-003 criterion 12).
            diagnostics_.recordComposited(accepted);
            logLine(L"Composited frame accepted for request %llu.",
                    static_cast<unsigned long long>(requestId.value()));
        },
        Qt::QueuedConnection);
}
