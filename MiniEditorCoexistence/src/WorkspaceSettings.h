#pragma once

#include "ProjectState.h"

#include <filesystem>
#include <optional>

// User-specific workspace preferences. This is deliberately separate from a
// portable video-editor project file: it describes the user's UI workspace.
struct WorkspaceSettings {
    int mediaLibraryWidth = 340;
    int propertiesWidth = 310;
    int timelineHeight = 270;
    int selectedAssetIndex = 0;
    TimelineViewState timelineViewState;
};

// A tiny framework-neutral JSON store. It works in both the Qt-enabled and
// pure-MFC builds, so neither UI framework becomes the settings owner.
class WorkspaceSettingsStore final
{
public:
    static std::optional<WorkspaceSettings> load();
    static bool save(const WorkspaceSettings &settings);
    static std::filesystem::path filePath();
};
