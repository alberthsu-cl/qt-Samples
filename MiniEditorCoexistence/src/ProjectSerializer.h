#pragma once

#include "EditorProject.h"

#include <filesystem>
#include <optional>
#include <string>

// File I/O boundary for portable .mini-editor.json project documents. This
// class has no MFC or Qt dependency, which keeps it usable by core tests and
// by a later Qt-native shell.
class ProjectSerializer final
{
public:
    static bool save(const std::filesystem::path &path,
                     const EditorProject &project,
                     std::wstring *errorMessage = nullptr);
    static std::optional<EditorProject> load(const std::filesystem::path &path,
                                             std::size_t expectedAssetCount,
                                             std::wstring *errorMessage = nullptr);
};
