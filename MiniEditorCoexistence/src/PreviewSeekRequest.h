#pragma once

#include "MediaPlaybackPlan.h"

#include <cstdint>
#include <optional>

class EditorSession;
class MediaLibrary;

// Immutable intent produced after the UI has updated EditorSession. The
// decoder must use this snapshot instead of resolving mutable editor state
// again while an asynchronous seek is in flight.
struct PreviewSeekRequest {
    std::uint64_t requestId = 0;
    int timelineFrame = 0;
    MediaPlaybackPlan playbackPlan;

    bool isValid() const;
};

// Completion data keeps the request identity attached to a decoded frame.
// A backend may receive completions out of order and must accept only the
// result belonging to its newest request.
struct PreviewSeekResult {
    std::uint64_t requestId = 0;
    int sourceFrame = 0;
};

class PreviewSeekRequestResolver final
{
public:
    static PreviewSeekRequest resolve(std::uint64_t requestId,
                                      const EditorSession &session,
                                      const MediaLibrary &mediaLibrary);
};

class PreviewSeekRequestTracker final
{
public:
    bool begin(const PreviewSeekRequest &request);
    bool accepts(const PreviewSeekResult &result) const;
    const std::optional<PreviewSeekRequest> &current() const;
    void complete(std::uint64_t requestId);
    void clear();

private:
    std::uint64_t latestRequestId_ = 0;
    std::optional<PreviewSeekRequest> currentRequest_;
};
