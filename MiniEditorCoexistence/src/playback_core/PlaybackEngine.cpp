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

void PlaybackEngine::publishStatus(std::optional<PlaybackCommandRejected> rejection)
{
    std::optional<PlaybackStatus> publishedStatus;
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        latestStatus_ = session_.status();
        publishedStatus = latestStatus_;
    }
    if (rejection) {
        std::lock_guard<std::mutex> lock(rejectionsMutex_);
        rejections_.push_back(*rejection);
    }

    // Published outside every lock above: a real sink must not block or
    // call back into the engine, but this class does not depend on that --
    // it just avoids holding its own locks longer than necessary.
    if (eventSink_) {
        eventSink_->publish(PlaybackEvent{*publishedStatus});
        if (rejection)
            eventSink_->publish(PlaybackEvent{*rejection});
    }
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
        } else {
            // A media-failure (or other future) observation: applied via
            // PlaybackSession's own identity check (a stale sessionId/
            // generation is a no-op), never a command, so it can never be
            // "rejected" -- only its resulting status is published.
            const auto &failure = std::get<QueuedFailure>(item);
            session_.reportFailure(failure.sessionId, failure.generation, failure.error);
            publishStatus(std::nullopt);
        }
    }
}

} // namespace mini_editor::playback_core
