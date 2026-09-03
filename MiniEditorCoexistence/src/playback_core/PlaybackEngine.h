#pragma once

#include "PlaybackSession.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace mini_editor::playback_core {

// ADR-005: one engine thread owns PlaybackSession's mutable state; all
// commands are serialized through one queue, and the queue never executes a
// caller's own callback inline on the producer thread. This milestone (M4-02)
// only introduces the thread and queue -- PlaybackSession's own rules are
// unchanged from Milestone 3. There is still no decoder/audio/compositor
// worker (M4-03/M4-04) and no UI notification bridge (M4-05); rejections and
// status are exposed here only as plain thread-safe accessors a test (or a
// future adapter) polls.
class PlaybackEngine final {
public:
    PlaybackEngine(PlaybackSource initialSource, const IPlaybackClock &clock);
    ~PlaybackEngine();

    PlaybackEngine(const PlaybackEngine &) = delete;
    PlaybackEngine &operator=(const PlaybackEngine &) = delete;

    // Enqueues a command; callable from any thread. Returns nullopt once the
    // command has been accepted into the queue -- its actual acceptance or
    // rejection by PlaybackSession happens later, on the engine thread, and
    // is observable afterward via status()/drainRejections(). Returns a
    // QueueClosed rejection immediately, without enqueueing, once a Shutdown
    // has already been accepted into the queue (ADR-002: "queue-closure
    // rejection may be returned immediately").
    std::optional<PlaybackCommandRejected> submit(PlaybackCommand command,
                                                  PlaybackCommandId commandId);

    // A thread-safe snapshot of the latest status published after the most
    // recently applied command.
    PlaybackStatus status() const;

    // Returns and clears every PlaybackCommandRejected produced (by submit()
    // or by the engine thread) since the last call, in submission order.
    std::vector<PlaybackCommandRejected> drainRejections();

    // Submits Shutdown (a no-op if the queue is already closed) and blocks
    // the calling thread until the engine thread has processed every command
    // queued ahead of it and exited. Safe to call more than once.
    void shutdownAndJoin();

private:
    struct QueuedCommand {
        PlaybackCommand command;
        PlaybackCommandId id;
    };

    void run();

    PlaybackSession session_; // mutated only on the engine thread (thread_).

    mutable std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<QueuedCommand> queue_;
    bool queueClosed_ = false;

    mutable std::mutex statusMutex_;
    PlaybackStatus latestStatus_;

    std::mutex rejectionsMutex_;
    std::vector<PlaybackCommandRejected> rejections_;

    std::thread thread_;
};

} // namespace mini_editor::playback_core
