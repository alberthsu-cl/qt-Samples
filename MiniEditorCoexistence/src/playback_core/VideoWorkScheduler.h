#pragma once

#include "VideoWork.h"

#include <functional>
#include <mutex>
#include <optional>
#include <utility>

namespace mini_editor::playback_core {

// ADR-003's bounded latest-wins video policy: at most one decode/composition
// request in flight, at most one newer request pending, a newer valid
// pending request replaces the older one (releasing it immediately), and an
// in-flight result that completes after a newer request has superseded it
// is discarded rather than composited. Correctness never depends on the
// decoder stopping immediately (ADR-003) -- this scheduler tolerates a
// superseded in-flight decode finishing late by discarding its result, not
// by trying to cancel it.
//
// This is independent of PreviewPresentationCoordinator: this class bounds
// *decode* work by decode identity, while the coordinator separately gates
// whether a successfully composited frame is still the *presentation*'s
// current want. A caller normally checks both before treating a
// CompositedVideoFrame as presentable.
class VideoWorkScheduler final {
public:
    VideoWorkScheduler(IVideoDecodeService &decoder, IVideoCompositor &compositor,
                       std::function<void(CompositedVideoFrame)> onFramePresented);

    // Requests a frame for one presentation want. Starts decoding
    // immediately if nothing is in flight; otherwise becomes the single
    // pending request, replacing any older pending request.
    void requestFrame(VideoDecodeRequest decodeRequest, FramePresentationRequest presentationRequest);

    bool hasInFlightWork() const;
    bool hasPendingWork() const;

private:
    void startDecode(VideoDecodeRequest decodeRequest, FramePresentationRequest presentationRequest);
    void onDecoded(VideoDecodeRequest requestThatCompleted, FramePresentationRequest presentationRequest,
                   DecodedVideoFrame frame);

    IVideoDecodeService &decoder_;
    IVideoCompositor &compositor_;
    std::function<void(CompositedVideoFrame)> onFramePresented_;

    mutable std::mutex mutex_;
    std::optional<VideoDecodeRequest> inFlightRequest_;
    std::optional<std::pair<VideoDecodeRequest, FramePresentationRequest>> pendingWork_;
};

} // namespace mini_editor::playback_core
