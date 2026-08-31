#include "PreviewSeekRequest.h"

#include "EditorSession.h"
#include "MediaLibrary.h"

bool PreviewSeekRequest::isValid() const
{
    return requestId != 0;
}

PreviewSeekRequest PreviewSeekRequestResolver::resolve(
    std::uint64_t requestId, const EditorSession &session,
    const MediaLibrary &mediaLibrary)
{
    return {
        requestId,
        session.playbackState().currentFrame,
        MediaPlaybackPlanResolver::resolve(session, mediaLibrary)
    };
}

bool PreviewSeekRequestTracker::begin(const PreviewSeekRequest &request)
{
    if (!request.isValid() || request.requestId <= latestRequestId_)
        return false;

    latestRequestId_ = request.requestId;
    currentRequest_ = request;
    return true;
}

bool PreviewSeekRequestTracker::accepts(const PreviewSeekResult &result) const
{
    return currentRequest_.has_value()
        && result.requestId == currentRequest_->requestId;
}

const std::optional<PreviewSeekRequest> &PreviewSeekRequestTracker::current() const
{
    return currentRequest_;
}

void PreviewSeekRequestTracker::complete(std::uint64_t requestId)
{
    if (currentRequest_ && currentRequest_->requestId == requestId)
        currentRequest_.reset();
}

void PreviewSeekRequestTracker::clear()
{
    currentRequest_.reset();
}
