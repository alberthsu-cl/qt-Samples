#pragma once

#include "PlaybackEventSink.h"
#include "PlaybackSession.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <variant>
#include <vector>

namespace mini_editor::playback_core {

// ADR-005: one engine thread owns PlaybackSession's mutable state; all
// commands are serialized through one queue, and the queue never executes a
// caller's own callback inline on the producer thread. PlaybackSession's own
// rules are unchanged from Milestone 3. There is still no decoder/audio/
// compositor worker driving the queue (that is Milestone 4's own adapter's
// job to call submit()); rejections and status are exposed as plain
// thread-safe accessors a test (or adapter) can poll, and, since M4-05, are
// also pushed to an optional IPlaybackEventSink so a real UI does not have
// to poll continuously.
class PlaybackEngine final {
public:
    // eventSink, if non-null, receives every status/rejection this engine
    // produces (ADR-007's UI notification port). It must outlive this
    // PlaybackEngine; publish() is called from the engine thread, so a real
    // sink must be safe to call from any thread without blocking (a plain
    // UiNotificationQueue already satisfies this).
    PlaybackEngine(PlaybackSource initialSource, const IPlaybackClock &clock,
                  IPlaybackEventSink *eventSink = nullptr);
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

    // ADR-002: a validated failure observation, entered into the SAME
    // serialized queue submit() uses -- callable from any thread (a real
    // decoder/worker reports failures from its own thread), never applied
    // inline on the calling thread, and always processed on the engine
    // thread in submission order alongside ordinary commands. A stale
    // sessionId/generation is discarded by PlaybackSession::reportFailure()
    // itself, exactly like any other observation; this method only gets it
    // there safely. Dropped without effect if the queue is already closed
    // (nothing left to process it). The resulting status (Failed + error,
    // or unchanged if stale) is published the same way an applied command's
    // status is -- via status()/drainRejections() and, if attached, the
    // event sink.
    void reportFailure(PlaybackSessionId sessionId, PlaybackGeneration generation,
                       PlaybackError error);

    // Asks the engine thread to look at the clock: applies ADR-002's natural
    // completion if the sequence has finished, and refreshes the snapshot
    // status() returns.
    //
    // A free-running transport applies no commands, so without this the
    // cached status stays frozen at whatever the last command produced --
    // the playhead appears not to move at all, and the end of the sequence
    // is never noticed. Enters the same serialized queue as commands and
    // failures, so it is an observation, not a ninth ADR-002 command.
    //
    // Callable from any thread and never blocks, which means status() is
    // still up to one observation behind when it returns. That is the price
    // of ADR-005's "the GUI thread must not block waiting for the engine",
    // and at a presentation tick's cadence it is one tick of lag.
    void observeClock();

    // A thread-safe snapshot of the latest status published after the most
    // recently applied command or clock observation.
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

    struct QueuedFailure {
        PlaybackSessionId sessionId;
        PlaybackGeneration generation;
        PlaybackError error;
    };

    // An observation with no payload: "look at the clock."
    struct QueuedClockObservation {};

    using QueueItem = std::variant<QueuedCommand, QueuedFailure, QueuedClockObservation>;

    void run();
    // Refreshes latestStatus_ from session_ and pushes it (and, if present,
    // rejection) to eventSink_. Called on the engine thread after every
    // processed queue item.
    void publishStatus(std::optional<PlaybackCommandRejected> rejection);
    // Recomputes latestStatus_ from session_, waking the sink only when the
    // caller says the change is one the UI has to react to.
    void refreshStatus(bool notifySink);

    PlaybackSession session_; // mutated only on the engine thread (thread_).

    mutable std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<QueueItem> queue_;
    bool queueClosed_ = false;

    mutable std::mutex statusMutex_;
    PlaybackStatus latestStatus_;

    std::mutex rejectionsMutex_;
    std::vector<PlaybackCommandRejected> rejections_;

    IPlaybackEventSink *eventSink_ = nullptr;

    std::thread thread_;
};

// The step every media-worker error handler performs identically: tag the
// failure with the engine's *current* session/generation and enter it into
// the engine's serialized queue. Shared by the Qt adapters (M4-07/M4-08) so
// the call shape lives in one place with one deterministic test, instead of
// being duplicated untested in each adapter. Framework-neutral by design --
// callers convert their own error type to a plain message first.
void reportWorkerFailure(PlaybackEngine &engine, std::string message);

} // namespace mini_editor::playback_core
