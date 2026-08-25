#include "ProjectSerializer.h"

#include <fstream>
#include <exception>
#include <regex>
#include <algorithm>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

constexpr int kCurrentFormatVersion = 5;

std::string utf8FromWide(const std::wstring &value)
{
    if (value.empty())
        return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                          result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring wideFromUtf8(const std::string &value)
{
    if (value.empty())
        return {};
    const int size = ::MultiByteToWideChar(CP_UTF8, 0, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                          result.data(), size);
    return result;
}

void setError(std::wstring *errorMessage, const wchar_t *message)
{
    if (errorMessage != nullptr)
        *errorMessage = message;
}

const char *positionName(ClipPosition position)
{
    switch (position) {
    case ClipPosition::Center:      return "Center";
    case ClipPosition::TopLeft:     return "TopLeft";
    case ClipPosition::TopRight:    return "TopRight";
    case ClipPosition::BottomLeft:  return "BottomLeft";
    case ClipPosition::BottomRight: return "BottomRight";
    }

    return "Center";
}

std::optional<ClipPosition> positionFromName(const std::string &name)
{
    if (name == "Center")
        return ClipPosition::Center;
    if (name == "TopLeft")
        return ClipPosition::TopLeft;
    if (name == "TopRight")
        return ClipPosition::TopRight;
    if (name == "BottomLeft")
        return ClipPosition::BottomLeft;
    if (name == "BottomRight")
        return ClipPosition::BottomRight;
    return std::nullopt;
}

const char *trackName(TimelineTrackType trackType)
{
    return trackType == TimelineTrackType::Audio ? "Audio" : "Video";
}

std::optional<TimelineTrackType> trackFromName(const std::string &name)
{
    if (name == "Video")
        return TimelineTrackType::Video;
    if (name == "Audio")
        return TimelineTrackType::Audio;
    return std::nullopt;
}

const char *mediaKindName(MediaKind kind)
{
    switch (kind) {
    case MediaKind::Video: return "Video";
    case MediaKind::Audio: return "Audio";
    case MediaKind::Image: return "Image";
    }
    return "Video";
}

std::optional<MediaKind> mediaKindFromName(const std::string &name)
{
    if (name == "Video") return MediaKind::Video;
    if (name == "Audio") return MediaKind::Audio;
    if (name == "Image") return MediaKind::Image;
    return std::nullopt;
}

std::optional<int> integerValue(const std::string &object, const char *key)
{
    const std::regex expression(std::string("\\\"") + key +
                                "\\\"\\s*:\\s*(-?\\d+)");
    std::smatch match;
    if (!std::regex_search(object, match, expression))
        return std::nullopt;

    try {
        return std::stoi(match[1].str());
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

std::optional<std::string> stringValue(const std::string &object, const char *key)
{
    const std::regex expression(std::string("\\\"") + key +
                                "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (!std::regex_search(object, match, expression))
        return std::nullopt;

    return match[1].str();
}

} // namespace

bool ProjectSerializer::save(const std::filesystem::path &path,
                             const EditorProject &project,
                             std::wstring *errorMessage)
{
    if (project.mediaAssets.empty()) {
        setError(errorMessage, L"The project has mismatched clip data.");
        return false;
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        setError(errorMessage, L"The project file could not be opened for writing.");
        return false;
    }

    output << "{\n"
           << "  \"formatVersion\": " << kCurrentFormatVersion << ",\n"
           << "  \"mediaAssets\": [\n";
    for (std::size_t index = 0; index < project.mediaAssets.size(); ++index) {
        const LibraryMediaAsset &asset = project.mediaAssets[index];
        output << "    { \"id\": " << asset.id
               << ", \"filePath\": \"" << asset.filePath.generic_string()
               << "\", \"displayName\": \"" << utf8FromWide(asset.displayName)
               << "\", \"kind\": \"" << mediaKindName(asset.kind)
               << "\", \"durationFrames\": " << asset.timelineDurationFrames
               << ", \"thumbnailColorRgb\": " << asset.thumbnailColorRgb << " }"
               << (index + 1 == project.mediaAssets.size() ? "\n" : ",\n");
    }
    output << "  ],\n"
           << "  \"timelineClips\": [\n";
    for (std::size_t index = 0; index < project.timelineItems.size(); ++index) {
        const TimelineClip &clip = project.timelineItems[index];
        output << "    { \"id\": " << clip.id
               << ", \"mediaAssetId\": " << clip.mediaAssetId
               << ", \"trackType\": \"" << trackName(clip.trackType)
               << "\", \"startFrame\": " << clip.state.startFrame
               << ", \"durationFrames\": " << clip.state.durationFrames
               << ", \"opacityPercent\": " << clip.settings.opacityPercent
               << ", \"scalePercent\": " << clip.settings.scalePercent
               << ", \"position\": \"" << positionName(clip.settings.position) << "\" }";
        output << (index + 1 == project.timelineItems.size() ? "\n" : ",\n");
    }
    output << "  ]\n"
           << "}\n";

    if (!output) {
        setError(errorMessage, L"The project file could not be written completely.");
        return false;
    }
    return true;
}

std::optional<EditorProject> ProjectSerializer::load(const std::filesystem::path &path,
                                                      std::size_t expectedAssetCount,
                                                      std::wstring *errorMessage)
{
    std::ifstream input(path);
    if (!input) {
        setError(errorMessage, L"The project file could not be opened.");
        return std::nullopt;
    }

    const std::string json((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    const auto formatVersion = integerValue(json, "formatVersion");
    if (!formatVersion || *formatVersion < 1 || *formatVersion > kCurrentFormatVersion) {
        setError(errorMessage, L"This is not a supported Mini Editor project file.");
        return std::nullopt;
    }

    EditorProject project;
    if (*formatVersion >= 4) {
        const std::regex mediaExpression("\\{\\s*\\\"id\\\"[^}]*\\\"filePath\\\"[^}]*\\}");
        for (std::sregex_iterator iterator(json.begin(), json.end(), mediaExpression), end;
             iterator != end; ++iterator) {
            const std::string object = iterator->str();
            const auto id = integerValue(object, "id");
            const auto filePath = stringValue(object, "filePath");
            const auto displayName = stringValue(object, "displayName");
            const auto kindName = stringValue(object, "kind");
            const auto durationFrames = integerValue(object, "durationFrames");
            const auto color = integerValue(object, "thumbnailColorRgb");
            const auto kind = kindName ? mediaKindFromName(*kindName) : std::nullopt;
            if (!id || *id <= 0 || !filePath || !displayName || !kind || !durationFrames
                || *durationFrames <= 0 || !color || *color < 0) {
                setError(errorMessage, L"A media asset in the project file is invalid.");
                return std::nullopt;
            }
            project.mediaAssets.push_back({ *id, std::filesystem::path(*filePath),
                                            wideFromUtf8(*displayName),
                                            *kind, *durationFrames,
                                            static_cast<std::uint32_t>(*color) });
        }
        if (project.mediaAssets.empty()) {
            setError(errorMessage, L"The project has no media assets.");
            return std::nullopt;
        }
    }

    const std::regex clipExpression("\\{\\s*\\\"opacityPercent\\\"[^}]*\\}");
    for (std::sregex_iterator iterator(json.begin(), json.end(), clipExpression), end;
         iterator != end; ++iterator) {
        const std::string clipObject = iterator->str();
        const auto opacityPercent = integerValue(clipObject, "opacityPercent");
        const auto scalePercent = integerValue(clipObject, "scalePercent");
        const auto position = stringValue(clipObject, "position");
        const auto startFrame = integerValue(clipObject, "startFrame");
        const auto durationFrames = integerValue(clipObject, "durationFrames");
        const auto clipPosition = position ? positionFromName(*position) : std::nullopt;

        if (!opacityPercent || !scalePercent || !clipPosition || !startFrame || !durationFrames) {
            setError(errorMessage, L"A clip in the project file is incomplete or invalid.");
            return std::nullopt;
        }

        project.clipSettings.push_back({ *opacityPercent, *scalePercent, *clipPosition });
        project.timelineClips.push_back({ *startFrame, *durationFrames });
    }

    if (*formatVersion == 5) {
        // These defaults support source preview while placement settings live
        // exclusively on TimelineClip in the v5 document.
        project.clipSettings.resize(project.mediaAssets.size());
        project.timelineClips.resize(project.mediaAssets.size());
    }
    const std::size_t requiredAssetCount = *formatVersion >= 4
        ? project.mediaAssets.size() : expectedAssetCount;
    if (project.clipSettings.size() != requiredAssetCount) {
        setError(errorMessage, L"This project does not match the sample media catalog.");
        return std::nullopt;
    }

    // Version 1 predates the independent TimelineModel. Keep old documents
    // compatible: their real timeline simply starts empty after loading.
    if (*formatVersion == 1)
        return project;

    const std::regex timelineClipExpression("\\{\\s*\\\"id\\\"[^}]*\\\"trackType\\\"[^}]*\\}");
    for (std::sregex_iterator iterator(json.begin(), json.end(), timelineClipExpression), end;
         iterator != end; ++iterator) {
        const std::string clipObject = iterator->str();
        const auto id = integerValue(clipObject, "id");
        // Version 2 stored a mutable catalog row. Version 3 stores the
        // stable media ID used by TimelineModel. The fixed v2 demo catalog
        // had IDs 1..6 in exactly row order, so it converts losslessly.
        const auto assetReference = *formatVersion == 2
            ? integerValue(clipObject, "mediaAssetIndex")
            : integerValue(clipObject, "mediaAssetId");
        const auto trackTypeName = stringValue(clipObject, "trackType");
        const auto startFrame = integerValue(clipObject, "startFrame");
        const auto durationFrames = integerValue(clipObject, "durationFrames");
        const auto trackType = trackTypeName ? trackFromName(*trackTypeName) : std::nullopt;

        if (!id || *id <= 0 || !assetReference || *assetReference < 0
            || !trackType || !startFrame || *startFrame < 0
            || !durationFrames || *durationFrames <= 0) {
            setError(errorMessage, L"A timeline clip in the project file is invalid.");
            return std::nullopt;
        }

        const int mediaAssetId = *formatVersion == 2 ? *assetReference + 1 : *assetReference;
        const bool hasAsset = *formatVersion >= 4
            ? std::any_of(project.mediaAssets.begin(), project.mediaAssets.end(),
                [mediaAssetId](const LibraryMediaAsset &asset) { return asset.id == mediaAssetId; })
            : mediaAssetId > 0 && mediaAssetId <= static_cast<int>(expectedAssetCount);
        if (!hasAsset) {
            setError(errorMessage, L"A timeline clip references an unknown media asset.");
            return std::nullopt;
        }

        ClipSettings settings;
        if (*formatVersion >= 5) {
            const auto opacity = integerValue(clipObject, "opacityPercent");
            const auto scale = integerValue(clipObject, "scalePercent");
            const auto positionNameValue = stringValue(clipObject, "position");
            const auto position = positionNameValue ? positionFromName(*positionNameValue) : std::nullopt;
            if (!opacity || !scale || !position) {
                setError(errorMessage, L"A timeline clip has incomplete placement settings.");
                return std::nullopt;
            }
            settings = { *opacity, *scale, *position };
        }
        project.timelineItems.push_back({ *id, mediaAssetId, *trackType,
                                          { *startFrame, *durationFrames }, settings });
    }

    // Version 4 stored placement settings once per source asset. During
    // migration, copy those settings into every timeline placement that
    // references the asset. Version 5 then persists each placement independently.
    if (*formatVersion == 4) {
        for (TimelineClip &clip : project.timelineItems) {
            const auto asset = std::find_if(project.mediaAssets.begin(), project.mediaAssets.end(),
                [&clip](const LibraryMediaAsset &candidate) {
                    return candidate.id == clip.mediaAssetId;
                });
            if (asset == project.mediaAssets.end())
                continue;
            const std::size_t index = static_cast<std::size_t>(asset - project.mediaAssets.begin());
            if (index < project.clipSettings.size())
                clip.settings = project.clipSettings[index];
        }
    }

    return project;
}
