#pragma once

#include "ProjectState.h"
#include "MediaLibrary.h"
#include "TimelineModel.h"

#include <cstddef>
#include <vector>

// The portable content of an editing project. It intentionally excludes UI
// preferences such as splitter dimensions and timeline zoom; those belong in
// WorkspaceSettings because they are per-user/per-machine choices.
struct EditorProject {
    std::vector<LibraryMediaAsset> mediaAssets;
    std::vector<ClipSettings> clipSettings;
    std::vector<TimelineClipState> timelineClips;
    std::vector<TimelineClip> timelineItems;

    static EditorProject createDefault(std::size_t assetCount);
};
