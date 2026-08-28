#pragma once

#include "MediaKind.h"

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// A source item in the project media library. IDs are stable: timeline clips
// refer to the ID rather than a mutable list row.
struct LibraryMediaAsset {
    int id = 0;
    std::filesystem::path filePath;
    std::wstring displayName;
    MediaKind kind = MediaKind::Video;
    int timelineDurationFrames = 0;
    std::uint32_t thumbnailColorRgb = 0x5078A0;
};

class MediaLibrary final
{
public:
    const std::vector<LibraryMediaAsset> &assets() const;
    const LibraryMediaAsset *findAsset(int assetId) const;
    int addKnownAsset(const std::filesystem::path &path, MediaKind kind,
                      int timelineDurationFrames, std::uint32_t thumbnailColorRgb);
    bool replaceAssets(const std::vector<LibraryMediaAsset> &assets);
    bool updateAssetDuration(int assetId, int timelineDurationFrames);
    std::optional<int> addFile(const std::filesystem::path &path);
    bool removeAsset(int assetId);

private:
    static std::optional<MediaKind> inferKind(const std::filesystem::path &path);
    static int defaultDurationFrames(MediaKind kind);

    std::vector<LibraryMediaAsset> assets_;
    int nextAssetId_ = 1;
};
