#include "PlaybackEngine.h"

#include <utility>

namespace mini_editor::playback_core {

PlaybackEngine::PlaybackEngine(PlaybackSource initialSource, const IPlaybackClock &clock,
                               IPlaybackEventSink *eventSink)
    : session_(std::move(initialSource), clock)
    , latestStatus_(session_.status())
    , eventSink_(eventSink)
    , thread_(&PlaybackEngine::run, this)
{
}

PlaybackEngine::~PlaybackEngine()
{
    shutdownAndJoin();
}

std::optional<PlaybackCommandRejected> PlaybackEngine::submit(PlaybackCommand command,
                                                              PlaybackCommandId commandId)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (queueClosed_) {
            const PlaybackCommandRejected rejected{commandId, PlaybackRejectReason::QueueClosed};
            {
                std::lock_guard<std::mutex> rejectionsLock(rejectionsMutex_);
                rejections_.push_back(rejected);
            }
            if (eventSink_)
                eventSink_->publish(PlaybackEvent{rejected});
            return rejected;
        }

        if (std::holds_alternative<Shutdown>(command))
            queueClosed_ = true;

        queue_.push_back(QueueItem{QueuedCommand{std::move(command), commandId}});
    }
    queueCv_.notify_one();
    return std::nullopt;
}

void PlaybackEngine::reportFailure(PlaybackSessionId sessionId, PlaybackGeneration generation,
                                   PlaybackError error)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (queueClosed_)
            return; // Nothing left to process it -- the engine thread has exited or is exiting.

        queue_.push_back(QueueItem{QueuedFailure{sessionId, generation, std::move(error)}});
    }
    queueCv_.notify_one();
}

void PlaybackEngine::observeClock()
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (queueClosed_)
            return;
        // Never let observations pile up behind a busy engine: one pending
        // "look at the clock" says everything a hundred of them would, and a
        // 15 ms presentation tick can outpace the queue during a slow
        // command.
        for (const QueueItem &item : queue_) {
            if (std::holds_alternative<QueuedClockObservation>(item))
                return;
        }
        queue_.push_back(QueuedClockObservation{});
    }
    queueCv_.notify_one();
}

PlaybackStatus PlaybackEngine::status() const
{
    std::lock_guard<std::mutex> lock(statusMutex_);
    return latestStatus_;
}

std::vector<PlaybackCommandRejected> PlaybackEngine::drainRejections()
{
    std::lock_guard<std::mutex> lock(rejectionsMutex_);
    std::vector<PlaybackCommandRejected> drained;
    drained.swap(rejections_);
    return drained;
}

void PlaybackEngine::shutdownAndJoin()
{
    submit(Shutdown{}, PlaybackCommandId::create());
    if (thread_.joinable())
        thread_.join();
}

void PlaybackEngine::refreshStatus(bool notifySink)
{
    PlaybackStatus refreshed = [this] {
        std::lock_guard<std::mutex> lock(statusMutex_);
        latestStatus_ = session_.status();
        return latestStatus_;
    }();

    if (notifySink && eventSink_)
        eventSink_->publish(PlaybackEvent{refreshed});
}

void PlaybackEngine::publishStatus(std::optional<PlaybackCommandRejected> rejection)
{
    // Published outside every lock: a real sink must not block or call back
    // into the engine, but this class does not depend on that -- it just
    // avoids holding its own locks longer than necessary.
    refreshStatus(/*notifySink=*/true);

    if (rejection) {
        {
            std::lock_guard<std::mutex> lock(rejectionsMutex_);
            rejections_.push_back(*rejection);
        }
        if (eventSink_)
            eventSink_->publish(PlaybackEvent{*rejection});
    }
}

void reportWorkerFailure(PlaybackEngine &engine, std::string message)
{
    // One status() read; both identity fields come from that single snapshot.
    // If it has gone stale by the time the engine thread applies the report,
    // PlaybackSession discards it -- which is the intended behavior, not a
    // race to defend against here.
    const PlaybackStatus current = engine.status();
    engine.reportFailure(current.sessionId, current.generation,
                         PlaybackError{std::move(message)});
}

void PlaybackEngine::run()
{
    for (;;) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCv_.wait(lock, [this] { return !queue_.empty(); });
        QueueItem item = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();

        if (auto *command = std::get_if<QueuedCommand>(&item)) {
            const bool isShutdown = std::holds_alternative<Shutdown>(command->command);
            const std::optional<PlaybackCommandRejected> rejection =
                session_.applyCommand(command->command, command->id);

            publishStatus(rejection);

            if (isShutdown)
                break; // ADR-005 step 6: the engine thread exits after publishing the final status.
        } else if (auto *failure = std::get_if<QueuedFailure>(&item)) {
            // A media-failure observation: applied via PlaybackSession's own
            // identity check (a stale sessionId/generation is a no-op), never
            // a command, so it can never be "rejected" -- only its resulting
            // status is published.
            session_.reportFailure(failure->sessionId, failure->generation, failure->error);
            publishStatus(std::nullopt);
        } else {
            const PlaybackPhase phaseBefore = session_.status().phase;
            session_.observeClock();
            // Always refresh the cached status -- a moving playhead is the
            // whole reason this observation exists -- but only wake the UI
            // when something it must react to actually happened. Publishing
            // an event per tick would be a PostMessage every 15 ms for a
            // value the tick is about to read anyway.
            const bool completed = session_.status().phase != phaseBefore;
            refreshStatus(completed);
        }
    }
}

} // namespace mini_editor::playback_core
