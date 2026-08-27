#pragma once

#include "EditorSession.h"
#include "MediaLibrary.h"

// Framework-neutral application policy for timeline-focused user actions.
// MFC and Qt may both send intent here without duplicating rules for focus,
// playback bounds, clipboard commands, or source-asset lookup.
class TimelineEditingController final
{
public:
    TimelineEditingController(EditorSession &session,
                              const MediaLibrary &mediaLibrary);

    bool focusClip(int clipId, bool resetToBeginning);
    void focusFrame(int frame);
    void followPlaybackFrame();
    void focusEmptyTimeline();
    void selectSourceAsset(int assetIndex);
    void seekFocusedPreview(int frame);

    bool deleteClip(int clipId);
    bool splitAtHead();
    bool copy();
    bool cut();
    bool paste();
    bool duplicate();

    bool canCopy() const;
    bool canCut() const;
    bool canPaste() const;
    bool canDuplicate() const;
    bool canSplitAtHead() const;

    void synchronizePlaybackDuration(bool resetToBeginning);

private:
    int assetIndexForMediaAsset(int mediaAssetId) const;
    bool finishInsertedClip(int clipId);

    EditorSession &session_;
    const MediaLibrary &mediaLibrary_;
};
