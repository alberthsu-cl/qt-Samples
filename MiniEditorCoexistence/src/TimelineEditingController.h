#pragma once

#include "EditorSession.h"
#include "MediaLibrary.h"

#include <functional>

// Framework-neutral application policy for timeline-focused user actions.
// MFC and Qt may both send intent here without duplicating rules for focus,
// playback bounds, clipboard commands, or source-asset lookup.
class TimelineEditingController final
{
public:
    TimelineEditingController(EditorSession &session,
                              const MediaLibrary &mediaLibrary);

    // Where a timeline head move goes when the engine owns the transport.
    //
    // Every user action that repositions the head -- a ruler click, the
    // transport slider, returning to the timeline from a library asset,
    // finishing an insertion -- used to reach it through
    // EditorSession::seekTimeline(). M5-09 made that refuse for a routed
    // timeline, which silently froze the head: the intent still went into a
    // method that no longer moved anything. Routing the *requested* frame to
    // its owner keeps one entry point for all four actions.
    //
    // Unset means the legacy path, where seekTimeline() is still the owner.
    using TimelineSeekSink = std::function<void(int)>;
    void setRoutedTimelineSeekSink(TimelineSeekSink sink);

    bool focusClip(int clipId, bool resetToBeginning);
    void focusFrame(int frame);
    void followPlaybackFrame();
    void focusEmptyTimeline();
    void selectSourceAsset(int assetIndex);
    void seekFocusedPreview(int frame);
    bool insertMediaAsset(int mediaAssetId, int startFrame);

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
    // Moves the timeline head through whoever owns it.
    void seekTimelineHead(int frame);

    EditorSession &session_;
    const MediaLibrary &mediaLibrary_;
    TimelineSeekSink routedTimelineSeekSink_;
};
