#include "EngineSmokeTestSession.h"

#include <QVideoWidget>

#include <Windows.h>

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
    ::OutputDebugStringW(L"[EngineSmokeTest] ");
    ::OutputDebugStringW(buffer);
    ::OutputDebugStringW(L"\n");
}

const wchar_t *phaseName(PlaybackPhase phase)
{
    switch (phase) {
    case PlaybackPhase::Stopped: return L"Stopped";
    case PlaybackPhase::Seeking: return L"Seeking";
    case PlaybackPhase::Prerolling: return L"Prerolling";
    case PlaybackPhase::Playing: return L"Playing";
    case PlaybackPhase::Paused: return L"Paused";
    case PlaybackPhase::Failed: return L"Failed";
    }
    return L"Unknown";
}

} // namespace

EngineSmokeTestSession::EngineSmokeTestSession(HWND notifyTarget, UINT notifyMessage)
    : bridge_(notifyTarget, notifyMessage)
    , previewWindow_(std::make_unique<QVideoWidget>())
{
    engine_ = std::make_unique<PlaybackEngine>(
        PlaybackSource{SourceAssetPreview{MediaAssetId(1)}}, clock_, &bridge_);

    previewWindow_->setWindowTitle(QStringLiteral("Playback Engine Smoke Test"));
    previewWindow_->resize(640, 360);

    QObject::connect(&worker_, &QtPlaybackMediaWorker::mediaErrorOccurred, this,
                     [this](const QString &message) {
                         logLine(L"QMediaPlayer error: %s",
                                 reinterpret_cast<const wchar_t *>(message.utf16()));

                         // The worker never touches PlaybackSession/PlaybackEngine directly --
                         // it only emits this signal. Reporting the failure through the engine's
                         // serialized queue, tagged with whatever session/generation is current
                         // right now, is this handler's job (M4-07). A stale identity by the time
                         // this is processed is discarded by PlaybackSession itself.
                         reportWorkerFailure(*engine_, message.toStdString());
                     });

    logLine(L"Session constructed. sessionId=%llu",
            static_cast<unsigned long long>(engine_->status().sessionId.valueForDiagnostics()));
}

EngineSmokeTestSession::~EngineSmokeTestSession()
{
    logLine(L"Session shutting down.");
    if (engine_)
        engine_->shutdownAndJoin();
}

void EngineSmokeTestSession::openAndPlay(const QString &filePath)
{
    logLine(L"Opening: %s", reinterpret_cast<const wchar_t *>(filePath.utf16()));

    worker_.attachExternalVideoOutput(previewWindow_->videoSink());
    worker_.openSource(filePath);
    previewWindow_->show();

    engine_->submit(OpenSource{MediaAssetId(1), SourceTimestamp::fromMicroseconds(0),
                               SourceCompletionPolicy::HoldLastFrame},
                    PlaybackCommandId::create());
    engine_->submit(Play{}, PlaybackCommandId::create());
}

void EngineSmokeTestSession::onNotification()
{
    handleEvents(bridge_.drain());
}

void EngineSmokeTestSession::handleEvents(std::vector<PlaybackEvent> events)
{
    for (const PlaybackEvent &event : events) {
        if (const auto *status = std::get_if<PlaybackStatus>(&event)) {
            logLine(L"status: phase=%s generation=%llu statusSeq=%llu",
                    phaseName(status->phase),
                    static_cast<unsigned long long>(status->generation.value()),
                    static_cast<unsigned long long>(status->statusSeq.value()));
            if (status->error) {
                logLine(L"  error: %hs", status->error->message.c_str());
            }

            if (status->phase != lastAppliedPhase_) {
                lastAppliedPhase_ = status->phase;
                switch (status->phase) {
                case PlaybackPhase::Playing:
                    logLine(L"-> worker.play()");
                    worker_.play();
                    break;
                case PlaybackPhase::Paused:
                case PlaybackPhase::Stopped:
                    logLine(L"-> worker.pause()");
                    worker_.pause();
                    break;
                default:
                    break;
                }
            }
        } else {
            const auto &rejected = std::get<PlaybackCommandRejected>(event);
            logLine(L"REJECTED: commandId=%llu reason=%d",
                    static_cast<unsigned long long>(rejected.id.valueForDiagnostics()),
                    static_cast<int>(rejected.reason));
        }
    }
}
