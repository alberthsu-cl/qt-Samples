#pragma once

#include "ProjectState.h"

class EditorSession;
class MediaLibrary;

// Framework-neutral policy for converting the current editor state into the
// data a preview surface needs to render. MFC and a future Qt preview can use
// the same answer without knowing about each other's UI types.
class PreviewStateResolver final
{
public:
    static PreviewState resolve(const EditorSession &session,
                                const MediaLibrary &mediaLibrary);
};
