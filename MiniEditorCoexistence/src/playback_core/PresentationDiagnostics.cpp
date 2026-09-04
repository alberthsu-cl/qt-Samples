#include "PresentationDiagnostics.h"

namespace mini_editor::playback_core {

void PresentationDiagnostics::recordComposited(const CompositedVideoFrame &frame)
{
    (void)frame;
    std::lock_guard<std::mutex> lock(mutex_);
    ++compositedCount_;
}

void PresentationDiagnostics::recordPresented(const FramePresented &frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++presentedCount_;
    lastPresented_ = frame;
}

std::size_t PresentationDiagnostics::compositedCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return compositedCount_;
}

std::size_t PresentationDiagnostics::presentedCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return presentedCount_;
}

std::optional<FramePresented> PresentationDiagnostics::lastPresented() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return lastPresented_;
}

} // namespace mini_editor::playback_core
