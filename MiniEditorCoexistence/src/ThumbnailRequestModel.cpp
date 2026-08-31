#include "ThumbnailRequestModel.h"

#include <algorithm>

ThumbnailRequest ThumbnailRequestModel::libraryThumbnail(int mediaAssetId,
                                                          int widthPixels,
                                                          int heightPixels)
{
    return { mediaAssetId, 0, std::max(1, widthPixels), std::max(1, heightPixels) };
}

std::vector<ThumbnailRequest> ThumbnailRequestModel::timelineStrip(
    const TimelineClip &clip, MediaKind mediaKind, int visibleWidthPixels,
    int targetHeightPixels)
{
    if (clip.mediaAssetId <= 0 || mediaKind == MediaKind::Audio || visibleWidthPixels <= 0)
        return {};

    // Keep every timeline cell close to a 16:9 thumbnail. Zooming a clip
    // requests more cells instead of stretching a small, fixed set across it.
    constexpr int kTargetThumbnailWidthPixels = 96;
    constexpr int kMaximumThumbnailCount = 64;
    const int requestCount = std::clamp(
        (visibleWidthPixels + kTargetThumbnailWidthPixels - 1) / kTargetThumbnailWidthPixels,
        1, kMaximumThumbnailCount);

    std::vector<ThumbnailRequest> requests;
    requests.reserve(requestCount);
    for (int index = 0; index < requestCount; ++index) {
        const int sourceFrame = mediaKind == MediaKind::Image
            ? 0
            : clip.state.sourceInFrame
                + (clip.state.durationFrames * index) / requestCount;
        requests.push_back({ clip.mediaAssetId, sourceFrame,
                             kTargetThumbnailWidthPixels,
                             std::max(1, targetHeightPixels) });
    }
    return requests;
}
