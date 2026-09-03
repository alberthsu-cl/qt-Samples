#pragma once

#include "PlaybackCommand.h"

#include <optional>

namespace mini_editor::playback_core {

// ADR-002: "statusSeq increases monotonically within one PlaybackSessionId
// and restarts for a new session. A consumer accepts only a status newer
// than the last status it accepted for that session. A new session ID
// resets that comparison." This is that consumer-side rule, generalized
// away from PlaybackSession so any future observation/UI-adapter code has
// one already-tested currency check to call instead of reinventing it.
class PlaybackStatusGate final {
public:
    // Returns true and records this status as the latest accepted one if it
    // is for a new session, or a newer statusSeq within the current session.
    // Returns false (and leaves recorded state untouched) for a stale or
    // duplicate status.
    bool acceptIfNewer(const PlaybackStatus &status);

private:
    std::optional<PlaybackSessionId> sessionId_;
    std::optional<StatusSequenceNumber> lastAcceptedSeq_;
};

} // namespace mini_editor::playback_core
