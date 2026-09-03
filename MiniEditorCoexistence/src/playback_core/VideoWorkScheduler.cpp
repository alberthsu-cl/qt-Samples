#include "VideoWorkScheduler.h"

namespace mini_editor::playback_core {

VideoWorkScheduler::VideoWorkScheduler(IVideoDecodeService &decoder, IVideoCompositor &compositor,
                                       std::function<void(CompositedVideoFrame)> onFramePresented)
    : decoder_(decoder)
    , compositor_(compositor)
    , onFramePresented_(std::move(onFramePresented))
{
}

void VideoWorkScheduler::requestFrame(VideoDecodeRequest decodeRequest,
                                      FramePresentationRequest presentationRequest)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (!inFlightRequest_) {
        inFlightRequest_ = decodeRequest;
        lock.unlock();
        startDecode(std::move(decodeRequest), std::move(presentationRequest));
        return;
    }

    // A newer request replaces (and releases) any older pending request
    // immediately -- there is nothing more to "release" for these
    // resource-free fake/value types, but this is the point where a real
    // implementation would drop the superseded snapshot/frame references.
    pendingWork_ = std::make_pair(std::move(decodeRequest), std::move(presentationRequest));
}

void VideoWorkScheduler::startDecode(VideoDecodeRequest decodeRequest,
                                     FramePresentationRequest presentationRequest)
{
    VideoDecodeRequest requestCopy = decodeRequest;
    decoder_.requestDecode(
        std::move(decodeRequest),
        [this, requestCopy, presentationRequest](DecodedVideoFrame frame) mutable {
            onDecoded(std::move(requestCopy), std::move(presentationRequest), std::move(frame));
        });
}

void VideoWorkScheduler::onDecoded(VideoDecodeRequest requestThatCompleted,
                                   FramePresentationRequest presentationRequest,
                                   DecodedVideoFrame frame)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (!inFlightRequest_ || *inFlightRequest_ != requestThatCompleted) {
        return; // Stale or duplicate completion (ADR-003): discard silently.
    }
    inFlightRequest_.reset();

    if (pendingWork_) {
        auto [nextDecode, nextPresentation] = std::move(*pendingWork_);
        pendingWork_.reset();
        inFlightRequest_ = nextDecode;
        lock.unlock();
        startDecode(std::move(nextDecode), std::move(nextPresentation));
        return;
    }

    lock.unlock();
    compositor_.composite(std::move(frame), std::move(presentationRequest),
                          [this](CompositedVideoFrame composited) {
                              onFramePresented_(std::move(composited));
                          });
}

bool VideoWorkScheduler::hasInFlightWork() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return inFlightRequest_.has_value();
}

bool VideoWorkScheduler::hasPendingWork() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return pendingWork_.has_value();
}

} // namespace mini_editor::playback_core
