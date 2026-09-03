#include "PlaybackEngine.h"

#include <utility>

namespace mini_editor::playback_core {

PlaybackEngine::PlaybackEngine(PlaybackSource initialSource, const IPlaybackClock &clock)
    : session_(std::move(initialSource), clock)
    , latestStatus_(session_.status())
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
            std::lock_guard<std::mutex> rejectionsLock(rejectionsMutex_);
            rejections_.push_back(rejected);
            return rejected;
        }

        if (std::holds_alternative<Shutdown>(command))
            queueClosed_ = true;

        queue_.push_back(QueuedCommand{std::move(command), commandId});
    }
    queueCv_.notify_one();
    return std::nullopt;
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

void PlaybackEngine::run()
{
    for (;;) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCv_.wait(lock, [this] { return !queue_.empty(); });
        QueuedCommand item = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();

        const bool isShutdown = std::holds_alternative<Shutdown>(item.command);
        const std::optional<PlaybackCommandRejected> rejection =
            session_.applyCommand(item.command, item.id);

        {
            std::lock_guard<std::mutex> lock(statusMutex_);
            latestStatus_ = session_.status();
        }
        if (rejection) {
            std::lock_guard<std::mutex> lock(rejectionsMutex_);
            rejections_.push_back(*rejection);
        }

        if (isShutdown)
            break; // ADR-005 step 6: the engine thread exits after publishing the final status.
    }
}

} // namespace mini_editor::playback_core
