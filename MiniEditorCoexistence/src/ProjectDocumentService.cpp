#include "ProjectDocumentService.h"

#include "EditorProject.h"
#include "EditorSession.h"
#include "MediaLibrary.h"
#include "ProjectSerializer.h"

#include <algorithm>
#include <utility>

ProjectDocumentService::ProjectDocumentService(EditorSession &session,
                                               MediaLibrary &mediaLibrary)
    : session_(session), mediaLibrary_(mediaLibrary)
{
}

void ProjectDocumentService::setDefaultMediaLibrary(
    const MediaLibrary &defaultMediaLibrary)
{
    defaultMediaLibrary_ = defaultMediaLibrary;
    defaultMediaAssetCount_ = static_cast<int>(defaultMediaLibrary_.assets().size());
    protectedMediaAssetCount_ = defaultMediaAssetCount_;
}

ProjectDocumentResult ProjectDocumentService::createNewProject()
{
    if (defaultMediaAssetCount_ <= 0) {
        return failure(ProjectDocumentError::InvalidMediaLibrary,
                       L"The default media library has not been initialized.");
    }

    mediaLibrary_ = defaultMediaLibrary_;
    protectedMediaAssetCount_ = defaultMediaAssetCount_;
    session_.replaceProject(EditorProject::createDefault(defaultMediaAssetCount_));
    return success();
}

ProjectDocumentResult ProjectDocumentService::save(const std::filesystem::path &path)
{
    EditorProject project = session_.projectSnapshot();
    project.mediaAssets = mediaLibrary_.assets();

    std::wstring errorMessage;
    if (!ProjectSerializer::save(path, project, &errorMessage)) {
        return failure(ProjectDocumentError::SerializationFailed,
                       std::move(errorMessage));
    }

    session_.markProjectSaved();
    return success();
}

ProjectDocumentResult ProjectDocumentService::load(const std::filesystem::path &path)
{
    std::wstring errorMessage;
    const auto project = ProjectSerializer::load(
        path, static_cast<std::size_t>(defaultMediaAssetCount_), &errorMessage);
    if (!project) {
        return failure(ProjectDocumentError::SerializationFailed,
                       std::move(errorMessage));
    }

    // Validate replacement media in a temporary catalog before changing the
    // live project. A malformed catalog must not partially open a project.
    MediaLibrary replacementLibrary = mediaLibrary_;
    int replacementProtectedCount = protectedMediaAssetCount_;
    if (!project->mediaAssets.empty()) {
        replacementLibrary = MediaLibrary{};
        if (!replacementLibrary.replaceAssets(project->mediaAssets)) {
            return failure(ProjectDocumentError::InvalidMediaLibrary,
                           L"The project media library is invalid.");
        }
        replacementProtectedCount = std::min(
            defaultMediaAssetCount_, static_cast<int>(replacementLibrary.assets().size()));
    }

    mediaLibrary_ = std::move(replacementLibrary);
    protectedMediaAssetCount_ = replacementProtectedCount;
    session_.replaceProject(*project);
    return success();
}

ProjectDocumentResult ProjectDocumentService::importMedia(
    const std::filesystem::path &path)
{
    if (!mediaLibrary_.addFile(path)) {
        return failure(ProjectDocumentError::UnsupportedMedia,
                       L"That file type is not supported by this sample.");
    }

    session_.addMediaAsset();
    return success();
}

ProjectDocumentResult ProjectDocumentService::removeMedia(int assetIndex,
                                                           int mediaAssetId)
{
    const auto &assets = mediaLibrary_.assets();
    if (assetIndex < 0 || assetIndex >= static_cast<int>(assets.size())
        || assets[assetIndex].id != mediaAssetId) {
        return failure(ProjectDocumentError::MissingMedia,
                       L"The selected media item no longer exists.");
    }
    if (assetIndex < protectedMediaAssetCount_) {
        return failure(ProjectDocumentError::ProtectedMedia,
                       L"Built-in sample media cannot be removed.");
    }

    const bool usedByTimeline = std::any_of(
        session_.timelineModel().clips().begin(), session_.timelineModel().clips().end(),
        [mediaAssetId](const TimelineClip &clip) {
            return clip.mediaAssetId == mediaAssetId;
        });
    if (usedByTimeline) {
        return failure(ProjectDocumentError::MediaUsedByTimeline,
                       L"Remove this asset's timeline clips before removing it from the library.");
    }

    // The index and ID were just validated above, so both operations must
    // succeed together and keep source settings aligned with library rows.
    if (!mediaLibrary_.removeAsset(mediaAssetId) || !session_.removeMediaAsset(assetIndex)) {
        return failure(ProjectDocumentError::MissingMedia,
                       L"The selected media item could not be removed.");
    }
    return success();
}

int ProjectDocumentService::protectedMediaAssetCount() const
{
    return protectedMediaAssetCount_;
}

ProjectDocumentResult ProjectDocumentService::success()
{
    return {};
}

ProjectDocumentResult ProjectDocumentService::failure(
    ProjectDocumentError error, std::wstring message)
{
    return { error, std::move(message) };
}
