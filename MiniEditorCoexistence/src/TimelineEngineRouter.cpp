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
                                           SequenceId sequenceId)
    : bridge_(notifyTarget, notifyMessage)
    , engine_(std::make_unique<PlaybackEngine>(
          PlaybackSource{SequencePreview{sequenceId}}, clock_, &bridge_))
    , previewWindow_(std::make_unique<QVideoWidget>())
{
    previewWindow_->setWindowTitle(QStringLiteral("Timeline Preview (New Engine)"));
    previewWindow_->resize(640, 360);
    worker_.attachExternalVideoOutput(previewWindow_->videoSink());

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
                         const PlaybackStatus current = engine_->status();
                         engine_->reportFailure(current.sessionId, current.generation,
                                                PlaybackError{message.toStdString()});
                     });

    logLine(L"Router constructed. sessionId=%llu",
            static_cast<unsigned long long>(engine_->status().sessionId.valueForDiagnostics()));
}

TimelineEngineRouter::~TimelineEngineRouter()
{
    logLine(L"Router shutting down.");
    if (engine_)
        engine_->shutdownAndJoin();
}

void TimelineEngineRouter::installSnapshot(SequencePlaybackSnapshotPtr snapshot)
{
    if (!snapshot)
        return;

    currentFrameRate_ = snapshot->frameRate;
    currentDuration_ = snapshot->duration;

    engine_->submit(InstallSnapshot{snapshot}, PlaybackCommandId::create());

    // Single-clip preview scope for this milestone -- see the class comment.
    if (!snapshot->videoClips.empty()) {
        const PlaybackClip &firstClip = snapshot->videoClips.front();
        const auto mediaIt = std::find_if(snapshot->media.begin(), snapshot->media.end(),
            [&firstClip](const PlaybackMediaDescriptor &descriptor) {
                return descriptor.mediaAssetId == firstClip.mediaAssetId;
            });
        if (mediaIt != snapshot->media.end() && mediaIt->availability == MediaAvailability::Available) {
            logLine(L"Installed snapshot; opening first V1 clip's media for preview.");
            worker_.openSource(QString::fromStdString(mediaIt->immutableSourceLocator));
            if (!previewWindow_->isVisible())
                previewWindow_->show();
        }
    }
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

        if (status->phase != lastAppliedPhase_) {
            lastAppliedPhase_ = status->phase;
            switch (status->phase) {
            case PlaybackPhase::Playing:
                worker_.play();
                break;
            case PlaybackPhase::Paused:
            case PlaybackPhase::Stopped:
                worker_.pause();
                break;
            case PlaybackPhase::Failed:
                logLine(L"phase=Failed error=%hs",
                        status->error ? status->error->message.c_str() : "(none)");
                break;
            default:
                break;
            }
        }
    }
}
