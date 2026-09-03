#include "PlaybackStatusGate.h"

namespace mini_editor::playback_core {

bool PlaybackStatusGate::acceptIfNewer(const PlaybackStatus &status)
{
    const bool isNewSession = !sessionId_ || *sessionId_ != status.sessionId;
    if (!isNewSession && lastAcceptedSeq_ && !(*lastAcceptedSeq_ < status.statusSeq))
        return false;

    sessionId_ = status.sessionId;
    lastAcceptedSeq_ = status.statusSeq;
    return true;
}

} // namespace mini_editor::playback_core
