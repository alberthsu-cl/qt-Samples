#include "PlaybackEventSink.h"

namespace mini_editor::playback_core {

void UiNotificationQueue::publish(PlaybackEvent event)
{
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(std::move(event));
}

std::vector<PlaybackEvent> UiNotificationQueue::drain()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PlaybackEvent> drained;
    drained.swap(events_);
    return drained;
}

bool UiNotificationQueue::empty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.empty();
}

} // namespace mini_editor::playback_core
