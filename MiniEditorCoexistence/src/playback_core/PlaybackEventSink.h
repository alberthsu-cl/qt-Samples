#pragma once

#include "PlaybackCommand.h"

#include <mutex>
#include <variant>
#include <vector>

namespace mini_editor::playback_core {

// One published notification. Scoped to what this milestone's engine
// actually produces -- a status refresh, or a rejected command. Real
// decode/composition/frame-ready events (the target document's
// VideoFrameReady/PlaybackEnded/MediaFailed alternatives) already fold into
// PlaybackStatus's phase/error fields per ADR-002/Milestone 3, so a status
// publication already carries that information; a distinct frame-delivery
// event is a concern for whoever consumes CompositedVideoFrame directly
// (VideoWorkScheduler's own callback, from Milestone 4's M4-03/M4-04), not
// this general-purpose status/rejection channel.
using PlaybackEvent = std::variant<PlaybackStatus, PlaybackCommandRejected>;

// ADR-007: "The core exposes a thread-safe UI notification queue through
// the IPlaybackEventSink port. It does not call UI code inline and does not
// require a Qt or MFC event loop to advance playback." publish() must be
// safe to call from the engine thread and must never block or call into UI
// code itself.
class IPlaybackEventSink {
public:
    virtual ~IPlaybackEventSink() = default;

    virtual void publish(PlaybackEvent event) = 0;
};

// The framework-neutral half of ADR-005's engine-to-GUI hand-off: publish()
// (engine thread) only ever locks a mutex and appends -- no UI call, no
// blocking wait, no allocation beyond the vector's own growth. drain() (GUI
// thread) retrieves everything published since the last call, in order. An
// MFC or Qt adapter wraps this with the actual "wake the GUI thread" step
// (a posted Windows message, a queued Qt signal); this class knows nothing
// about either framework.
class UiNotificationQueue final : public IPlaybackEventSink {
public:
    void publish(PlaybackEvent event) override;

    std::vector<PlaybackEvent> drain();

    bool empty() const;

private:
    mutable std::mutex mutex_;
    std::vector<PlaybackEvent> events_;
};

} // namespace mini_editor::playback_core
