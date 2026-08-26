#include "WorkspaceSettings.h"

#include <Windows.h>

#include <fstream>
#include <regex>
#include <string>
#include <vector>

namespace {

std::filesystem::path localAppDataPath()
{
    const DWORD requiredSize = ::GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (requiredSize == 0)
        return {};

    std::vector<wchar_t> buffer(requiredSize);
    if (::GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), requiredSize) == 0)
        return {};

    return buffer.data();
}

std::optional<int> integerValue(const std::string &json, const char *key)
{
    const std::regex expression(std::string("\\\"") + key +
                                "\\\"\\s*:\\s*(-?\\d+)");
    std::smatch match;
    if (!std::regex_search(json, match, expression))
        return std::nullopt;

    try {
        return std::stoi(match[1].str());
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

std::optional<bool> booleanValue(const std::string &json, const char *key)
{
    const std::regex expression(std::string("\\\"") + key +
                                "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, expression))
        return std::nullopt;

    return match[1] == "true";
}

} // namespace

std::filesystem::path WorkspaceSettingsStore::filePath()
{
    const std::filesystem::path appData = localAppDataPath();
    if (appData.empty())
        return {};

    return appData / L"QtLearningSamples" / L"MiniEditorCoexistence" / L"workspace.json";
}

std::optional<WorkspaceSettings> WorkspaceSettingsStore::load()
{
    const std::filesystem::path path = filePath();
    std::ifstream input(path);
    if (!input)
        return std::nullopt;

    const std::string json((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    WorkspaceSettings settings;
    const auto mediaLibraryWidth = integerValue(json, "mediaLibraryWidth");
    const auto propertiesWidth = integerValue(json, "propertiesWidth");
    const auto timelineHeight = integerValue(json, "timelineHeight");
    const auto selectedAssetIndex = integerValue(json, "selectedAssetIndex");
    const auto zoomPercent = integerValue(json, "timelineZoomPercent");
    const auto isAudioTrackVisible = booleanValue(json, "isAudioTrackVisible");
    const auto isRippleEditingEnabled = booleanValue(json, "isRippleEditingEnabled");

    // A missing field means an older settings file. Keep its default rather
    // than failing the entire load, which makes future settings evolution safe.
    if (mediaLibraryWidth)
        settings.mediaLibraryWidth = *mediaLibraryWidth;
    if (propertiesWidth)
        settings.propertiesWidth = *propertiesWidth;
    if (timelineHeight)
        settings.timelineHeight = *timelineHeight;
    if (selectedAssetIndex)
        settings.selectedAssetIndex = *selectedAssetIndex;
    if (zoomPercent)
        settings.timelineViewState.zoomPercent = *zoomPercent;
    if (isAudioTrackVisible)
        settings.timelineViewState.isAudioTrackVisible = *isAudioTrackVisible;
    if (isRippleEditingEnabled)
        settings.timelineViewState.isRippleEditingEnabled = *isRippleEditingEnabled;

    return settings;
}

bool WorkspaceSettingsStore::save(const WorkspaceSettings &settings)
{
    const std::filesystem::path path = filePath();
    if (path.empty())
        return false;

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
        return false;

    std::ofstream output(path, std::ios::trunc);
    if (!output)
        return false;

    output << "{\n"
           << "  \"formatVersion\": 2,\n"
           << "  \"mediaLibraryWidth\": " << settings.mediaLibraryWidth << ",\n"
           << "  \"propertiesWidth\": " << settings.propertiesWidth << ",\n"
           << "  \"timelineHeight\": " << settings.timelineHeight << ",\n"
           << "  \"selectedAssetIndex\": " << settings.selectedAssetIndex << ",\n"
           << "  \"timelineZoomPercent\": " << settings.timelineViewState.zoomPercent << ",\n"
           << "  \"isAudioTrackVisible\": "
           << (settings.timelineViewState.isAudioTrackVisible ? "true" : "false") << ",\n"
           << "  \"isRippleEditingEnabled\": "
           << (settings.timelineViewState.isRippleEditingEnabled ? "true" : "false") << "\n"
           << "}\n";
    return static_cast<bool>(output);
}
