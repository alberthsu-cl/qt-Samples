#include "MediaLibrary.h"

#include <algorithm>
#include <cwctype>

namespace {

constexpr int kFramesPerSecond = 30;
constexpr int kStillImageDurationFrames = 3 * kFramesPerSecond;
constexpr int kDefaultVideoDurationFrames = 10 * kFramesPerSecond;
constexpr int kDefaultAudioDurationFrames = 30 * kFramesPerSecond;

std::wstring lowercase(std::wstring text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](wchar_t character) { return std::towlower(character); });
    return text;
}

} // namespace

const std::vector<LibraryMediaAsset> &MediaLibrary::assets() const
{
    return assets_;
}

const LibraryMediaAsset *MediaLibrary::findAsset(int assetId) const
{
    const auto iterator = std::find_if(assets_.begin(), assets_.end(),
        [assetId](const LibraryMediaAsset &asset) { return asset.id == assetId; });
    return iterator == assets_.end() ? nullptr : &*iterator;
}

std::optional<int> MediaLibrary::addFile(const std::filesystem::path &path)
{
    const auto kind = inferKind(path);
    if (!kind || path.filename().empty())
        return std::nullopt;

    const int assetId = nextAssetId_++;
    assets_.push_back({ assetId, path, path.filename().wstring(), *kind,
                        defaultDurationFrames(*kind) });
    return assetId;
}

bool MediaLibrary::removeAsset(int assetId)
{
    const auto iterator = std::find_if(assets_.begin(), assets_.end(),
        [assetId](const LibraryMediaAsset &asset) { return asset.id == assetId; });
    if (iterator == assets_.end())
        return false;

    assets_.erase(iterator);
    return true;
}

std::optional<MediaKind> MediaLibrary::inferKind(const std::filesystem::path &path)
{
    const std::wstring extension = lowercase(path.extension().wstring());
    if (extension == L".mp4" || extension == L".mov" || extension == L".mkv"
        || extension == L".avi") {
        return MediaKind::Video;
    }
    if (extension == L".mp3" || extension == L".wav" || extension == L".m4a"
        || extension == L".aac") {
        return MediaKind::Audio;
    }
    if (extension == L".jpg" || extension == L".jpeg" || extension == L".png"
        || extension == L".bmp") {
        return MediaKind::Image;
    }
    return std::nullopt;
}

int MediaLibrary::defaultDurationFrames(MediaKind kind)
{
    switch (kind) {
    case MediaKind::Video:
        return kDefaultVideoDurationFrames;
    case MediaKind::Audio:
        return kDefaultAudioDurationFrames;
    case MediaKind::Image:
        return kStillImageDurationFrames;
    }

    return kStillImageDurationFrames;
}
