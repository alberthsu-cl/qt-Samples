#pragma once

#include "MediaKind.h"
#include "ProjectState.h"

class EditorSession;
class MediaLibrary;

enum class ClipPropertiesTarget {
    MediaAsset,
    TimelineClip,
    EmptyTimeline
};

// One complete, framework-neutral snapshot for a Properties view. A host
// applies this atomically instead of coordinating several partly related UI
// setters and risking a panel that temporarily shows mixed selection state.
struct ClipPropertiesViewState {
    ClipPropertiesTarget target = ClipPropertiesTarget::MediaAsset;
    bool editingEnabled = false;
    MediaKind mediaKind = MediaKind::Video;
    // These identify a source-library item. A timeline placement keeps its
    // editable settings below, while a selected source shows this read-only
    // information as preparation for real media metadata in a later phase.
    std::wstring mediaDisplayName;
    std::wstring mediaFilePath;
    int durationFrames = 0;
    ClipSettings settings;
};

class ClipPropertiesStateResolver final
{
public:
    static ClipPropertiesViewState resolve(const EditorSession &session,
                                           const MediaLibrary &mediaLibrary);
};
