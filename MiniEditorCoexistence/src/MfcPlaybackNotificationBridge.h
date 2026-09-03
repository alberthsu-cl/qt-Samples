#pragma once

#include "playback_core/PlaybackEventSink.h"

#include <afxwin.h>

// ADR-007's MFC adapter, concretely: "It posts one Windows message to
// MainFrame when the UI notification queue becomes non-empty; the GUI
// thread drains that queue and updates MFC controls and any embedded Qt
// widgets." No PostMessage/ON_MESSAGE notification path existed anywhere in
// this codebase before Milestone 4 (M4-05) -- this is that mechanism, not an
// extension of one.
//
// publish() (called from the engine thread) only stores the event in the
// framework-neutral UiNotificationQueue and posts one Windows message; it
// never blocks and never calls into MFC/Qt UI code itself. drain() (called
// from MainFrame's message handler, on the GUI thread) retrieves everything
// published since the last call.
class MfcPlaybackNotificationBridge final : public mini_editor::playback_core::IPlaybackEventSink {
public:
    MfcPlaybackNotificationBridge(HWND targetWindow, UINT notificationMessage);

    void publish(mini_editor::playback_core::PlaybackEvent event) override;

    std::vector<mini_editor::playback_core::PlaybackEvent> drain();

private:
    mini_editor::playback_core::UiNotificationQueue queue_;
    HWND targetWindow_;
    UINT notificationMessage_;
};
