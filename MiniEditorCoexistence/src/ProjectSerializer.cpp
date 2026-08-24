#include "ProjectSerializer.h"

#include <fstream>
#include <exception>
#include <regex>

namespace {

constexpr int kCurrentFormatVersion = 1;

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
           << "  \"clips\": [\n";
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
    if (!formatVersion || *formatVersion != kCurrentFormatVersion) {
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

    return project;
}
