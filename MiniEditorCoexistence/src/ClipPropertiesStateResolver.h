#pragma once

#include "MediaKind.h"
#include "ProjectState.h"

class EditorSession;
class MediaLibrary;

// One complete, framework-neutral snapshot for a Properties view. A host
// applies this atomically instead of coordinating several partly related UI
// setters and risking a panel that temporarily shows mixed selection state.
struct ClipPropertiesViewState {
    bool editingEnabled = false;
    MediaKind mediaKind = MediaKind::Video;
    int durationFrames = 0;
    ClipSettings settings;
};

class ClipPropertiesStateResolver final
{
public:
    static ClipPropertiesViewState resolve(const EditorSession &session,
                                           const MediaLibrary &mediaLibrary);
};
