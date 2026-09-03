#pragma once

#include "MfcPlaybackNotificationBridge.h"
#include "QtPlaybackMediaWorker.h"
#include "playback_core/PlaybackEngine.h"
#include "playback_core/SteadyPlaybackClock.h"

#include <QObject>
#include <QString>

#include <memory>

class QVideoWidget;

// Manual validation scaffolding for Milestone 4 (M4-04/M4-05), reachable
// only through a debug hotkey gated by MINI_EDITOR_ENABLE_ENGINE_SMOKE_TEST
// (off by default). It exists so a human can watch and listen to the new
// PlaybackEngine + QtPlaybackMediaWorker + MfcPlaybackNotificationBridge
// pipeline actually work end to end, in a standalone preview window
// completely separate from the application's real preview surface --
// nothing here is reachable from normal application use, and no legacy
// playback code path is touched by any of it.
//
// The chain this proves: PlaybackEngine (its own thread, SteadyPlaybackClock,
// no timer) applies a command -> MfcPlaybackNotificationBridge posts a real
// Windows message to MainFrame -> MainFrame's handler drains the queue and
// translates the resulting PlaybackStatus into a play()/pause() call on
// QtPlaybackMediaWorker (its own thread, real QMediaPlayer). Nothing here
// polls a timer and nothing calls QCoreApplication::processEvents().
class EngineSmokeTestSession final : public QObject {
    Q_OBJECT

public:
    // notifyTarget/notifyMessage: MainFrame's HWND and the WM_APP+n message
    // it will post itself when the engine has something to report.
    EngineSmokeTestSession(HWND notifyTarget, UINT notifyMessage);
    ~EngineSmokeTestSession() override;

    // Opens filePath in the standalone preview window and starts playback
    // through the real engine (not directly through the Qt worker) --
    // the following play()/pause() calls on the worker happen only as a
    // reaction to the engine's own published status, via handleEvents().
    void openAndPlay(const QString &filePath);

    // Called by MainFrame's WM_APP handler. Drains the bridge, logs each
    // event (see the manual smoke-test checklist for exactly what should
    // appear), and, for a status whose phase actually changed, issues the
    // matching real QMediaPlayer call.
    void onNotification();

    QVideoWidget *previewWindow() const { return previewWindow_.get(); }

private:
    void handleEvents(std::vector<mini_editor::playback_core::PlaybackEvent> events);

    MfcPlaybackNotificationBridge bridge_;
    mini_editor::playback_core::SteadyPlaybackClock clock_;
    std::unique_ptr<mini_editor::playback_core::PlaybackEngine> engine_;
    QtPlaybackMediaWorker worker_;
    std::unique_ptr<QVideoWidget> previewWindow_;
    mini_editor::playback_core::PlaybackPhase lastAppliedPhase_ =
        mini_editor::playback_core::PlaybackPhase::Stopped;
};
