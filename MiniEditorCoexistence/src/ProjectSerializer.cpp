#include "ProjectSerializer.h"

#include <fstream>
#include <exception>
#include <regex>

namespace {

constexpr int kCurrentFormatVersion = 3;

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
    if (project.clipSettings.size() != project.timelineClips.size()) {
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
           << "  \"assetSettings\": [\n";
    for (std::size_t index = 0; index < project.clipSettings.size(); ++index) {
        const ClipSettings &settings = project.clipSettings[index];
        const TimelineClipState &timelineClip = project.timelineClips[index];
        output << "    { \"opacityPercent\": " << settings.opacityPercent
               << ", \"scalePercent\": " << settings.scalePercent
               << ", \"position\": \"" << positionName(settings.position)
               << "\", \"startFrame\": " << timelineClip.startFrame
               << ", \"durationFrames\": " << timelineClip.durationFrames << " }";
        output << (index + 1 == project.clipSettings.size() ? "\n" : ",\n");
    }
    output << "  ],\n"
           << "  \"timelineClips\": [\n";
    for (std::size_t index = 0; index < project.timelineItems.size(); ++index) {
        const TimelineClip &clip = project.timelineItems[index];
        output << "    { \"id\": " << clip.id
               << ", \"mediaAssetId\": " << clip.mediaAssetId
               << ", \"trackType\": \"" << trackName(clip.trackType)
               << "\", \"startFrame\": " << clip.state.startFrame
               << ", \"durationFrames\": " << clip.state.durationFrames << " }";
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

    const std::regex clipExpression("\\{\\s*\\\"opacityPercent\\\"[^}]*\\}");
    EditorProject project;
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

    if (project.clipSettings.size() != expectedAssetCount) {
        setError(errorMessage, L"This project does not match the sample media catalog.");
        return std::nullopt;
    }

    // Version 1 predates the independent TimelineModel. Keep old documents
    // compatible: their real timeline simply starts empty after loading.
    if (*formatVersion == 1)
        return project;

    const std::regex timelineClipExpression("\\{\\s*\\\"id\\\"[^}]*\\}");
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
        if (mediaAssetId <= 0 || mediaAssetId > static_cast<int>(expectedAssetCount)) {
            setError(errorMessage, L"A timeline clip references an unknown media asset.");
            return std::nullopt;
        }

        project.timelineItems.push_back({ *id, mediaAssetId, *trackType,
                                          { *startFrame, *durationFrames } });
    }

    return project;
}
