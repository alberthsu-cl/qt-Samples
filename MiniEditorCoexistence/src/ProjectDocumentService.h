#pragma once

#include "MediaLibrary.h"

#include <filesystem>
#include <string>

class EditorSession;

enum class ProjectDocumentError {
    None,
    UnsupportedMedia,
    ProtectedMedia,
    MediaUsedByTimeline,
    MissingMedia,
    InvalidMediaLibrary,
    SerializationFailed
};

// Plain result data returned to either an MFC or Qt shell. The UI decides
// whether an error is presented as a dialog, status message, or notification.
struct ProjectDocumentResult {
    ProjectDocumentError error = ProjectDocumentError::None;
    std::wstring message;

    bool succeeded() const { return error == ProjectDocumentError::None; }
};

// Framework-neutral project and media-library workflow. It keeps the session
// and media catalog consistent while leaving native file dialogs and prompts
// to the hosting UI.
class ProjectDocumentService final
{
public:
    ProjectDocumentService(EditorSession &session, MediaLibrary &mediaLibrary);

    void setDefaultMediaLibrary(const MediaLibrary &defaultMediaLibrary);
    ProjectDocumentResult createNewProject();
    ProjectDocumentResult save(const std::filesystem::path &path);
    ProjectDocumentResult load(const std::filesystem::path &path);
    ProjectDocumentResult importMedia(const std::filesystem::path &path);
    ProjectDocumentResult removeMedia(int assetIndex, int mediaAssetId);

    int protectedMediaAssetCount() const;

private:
    static ProjectDocumentResult success();
    static ProjectDocumentResult failure(ProjectDocumentError error,
                                         std::wstring message);

    EditorSession &session_;
    MediaLibrary &mediaLibrary_;
    MediaLibrary defaultMediaLibrary_;
    int defaultMediaAssetCount_ = 0;
    int protectedMediaAssetCount_ = 0;
};
