#include "ClipPropertiesStateResolver.h"

#include "EditorSession.h"
#include "MediaLibrary.h"

ClipPropertiesViewState ClipPropertiesStateResolver::resolve(
    const EditorSession &session, const MediaLibrary &mediaLibrary)
{
    ClipPropertiesViewState viewState;
    viewState.settings = session.selectedClipSettings();

    const TimelineClip *selectedClip = session.timelineModel().findClip(
        session.selectedTimelineClipId());
    if (selectedClip != nullptr) {
        viewState.target = ClipPropertiesTarget::TimelineClip;
        viewState.editingEnabled = true;
        viewState.durationFrames = selectedClip->state.durationFrames;
        if (const LibraryMediaAsset *asset =
                mediaLibrary.findAsset(selectedClip->mediaAssetId)) {
            viewState.mediaKind = asset->kind;
            viewState.mediaDisplayName = asset->displayName;
            viewState.mediaFilePath = asset->filePath.wstring();
        }
        return viewState;
    }

    viewState.target = session.isTimelineFocused()
        ? ClipPropertiesTarget::EmptyTimeline
        : ClipPropertiesTarget::MediaAsset;

    if (viewState.target != ClipPropertiesTarget::MediaAsset)
        return viewState;

    const auto &assets = mediaLibrary.assets();
    const int selectedAssetIndex = session.selectedAssetIndex();
    if (selectedAssetIndex >= 0
        && selectedAssetIndex < static_cast<int>(assets.size())) {
        const LibraryMediaAsset &asset = assets[selectedAssetIndex];
        viewState.mediaKind = asset.kind;
        viewState.mediaDisplayName = asset.displayName;
        viewState.mediaFilePath = asset.filePath.wstring();
        viewState.durationFrames = asset.timelineDurationFrames;
    }
    return viewState;
}
