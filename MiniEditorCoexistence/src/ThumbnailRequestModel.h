#pragma once

#include "MediaKind.h"
#include "TimelineModel.h"

#include <vector>

// A cache key for one visual representation of a source asset. It has no Qt
// dependency: a future thumbnail service can decode it to QImage, a GPU
// texture, or another platform-native image type.
struct ThumbnailRequest {
    int mediaAssetId = 0;
    int sourceFrame = 0;
    int targetWidthPixels = 0;
    int targetHeightPixels = 0;
};

// Defines which source frames the two editor surfaces need. It deliberately
// does not load files or hold pixel data; a UI-layer cache/service owns that.
class ThumbnailRequestModel final
{
public:
    static ThumbnailRequest libraryThumbnail(int mediaAssetId,
                                             int widthPixels = 128,
                                             int heightPixels = 72);
    static std::vector<ThumbnailRequest> timelineStrip(const TimelineClip &clip,
                                                        MediaKind mediaKind,
                                                        int visibleWidthPixels,
                                                        int targetHeightPixels = 54);
};
