#pragma once

#include "VideoWork.h"

#include <cstddef>
#include <mutex>
#include <optional>

namespace mini_editor::playback_core {

// ADR-003: "renderer acknowledgement updates presentation diagnostics only."
// This is that only. Composition readiness and surface commit are counted
// separately, because criterion 12 requires them to stay distinguishable --
// a frame that decoded and composited but never reached a surface is a
// different fact from one the user actually saw.
//
// Nothing here can move a playhead: it holds counters and the last
// acknowledgement, and has no reference to a session, a clock, or a queue.
// Thread-safe, because composition finishes on a worker thread while the
// surface commits on the GUI thread.
class PresentationDiagnostics final {
public:
    void recordComposited(const CompositedVideoFrame &frame);
    void recordPresented(const FramePresented &frame);

    std::size_t compositedCount() const;
    std::size_t presentedCount() const;
    std::optional<FramePresented> lastPresented() const;

private:
    mutable std::mutex mutex_;
    std::size_t compositedCount_ = 0;
    std::size_t presentedCount_ = 0;
    std::optional<FramePresented> lastPresented_;
};

} // namespace mini_editor::playback_core
