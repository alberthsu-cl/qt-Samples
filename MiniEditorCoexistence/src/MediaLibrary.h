#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

enum class MediaKind {
    Video,
    Audio,
    Image
};

// A source item in the project media library. IDs are stable: timeline clips
// refer to the ID rather than a mutable list row.
struct LibraryMediaAsset {
    int id = 0;
    std::filesystem::path filePath;
    std::wstring displayName;
    MediaKind kind = MediaKind::Video;
    int timelineDurationFrames = 0;
};

class MediaLibrary final
{
public:
    const std::vector<LibraryMediaAsset> &assets() const;
    const LibraryMediaAsset *findAsset(int assetId) const;
    std::optional<int> addFile(const std::filesystem::path &path);
    bool removeAsset(int assetId);

private:
    static std::optional<MediaKind> inferKind(const std::filesystem::path &path);
    static int defaultDurationFrames(MediaKind kind);

    std::vector<LibraryMediaAsset> assets_;
    int nextAssetId_ = 1;
};
