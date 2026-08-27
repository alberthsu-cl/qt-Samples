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
        viewState.editingEnabled = true;
        viewState.durationFrames = selectedClip->state.durationFrames;
        if (const LibraryMediaAsset *asset =
                mediaLibrary.findAsset(selectedClip->mediaAssetId)) {
            viewState.mediaKind = asset->kind;
        }
        return viewState;
    }

    const auto &assets = mediaLibrary.assets();
    const int selectedAssetIndex = session.selectedAssetIndex();
    if (selectedAssetIndex >= 0
        && selectedAssetIndex < static_cast<int>(assets.size())) {
        viewState.mediaKind = assets[selectedAssetIndex].kind;
    }
    return viewState;
}
