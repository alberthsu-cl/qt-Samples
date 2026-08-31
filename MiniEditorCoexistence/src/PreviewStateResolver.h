#pragma once

#include "ProjectState.h"
#include "TimelinePlaybackResolver.h"

class EditorSession;
class MediaLibrary;

// Framework-neutral policy for converting the current editor state into the
// data a preview surface needs to render. MFC and a future Qt preview can use
// the same answer without knowing about each other's UI types.
class PreviewStateResolver final
{
public:
    // The picture the timeline preview should decode. While stopped, a
    // focused video placement is the edit target even when the playhead is
    // elsewhere. During play/pause, the playhead owns the visible picture.
    // Renderers and media backends must use this same policy.
    static std::optional<ResolvedTimelineMedia> resolveTimelineVideo(
        const EditorSession &session, const MediaLibrary &mediaLibrary);

    static PreviewState resolve(const EditorSession &session,
                                const MediaLibrary &mediaLibrary);
};
