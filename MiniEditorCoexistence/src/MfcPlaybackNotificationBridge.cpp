#include "MfcPlaybackNotificationBridge.h"

MfcPlaybackNotificationBridge::MfcPlaybackNotificationBridge(HWND targetWindow, UINT notificationMessage)
    : targetWindow_(targetWindow)
    , notificationMessage_(notificationMessage)
{
}

void MfcPlaybackNotificationBridge::publish(mini_editor::playback_core::PlaybackEvent event)
{
    queue_.publish(std::move(event));
    ::PostMessage(targetWindow_, notificationMessage_, 0, 0);
}

std::vector<mini_editor::playback_core::PlaybackEvent> MfcPlaybackNotificationBridge::drain()
{
    return queue_.drain();
}
