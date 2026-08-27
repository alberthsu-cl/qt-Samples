#include "TimelineEditingController.h"

#include "TimelinePlaybackResolver.h"

#include <algorithm>

TimelineEditingController::TimelineEditingController(
    EditorSession &session, const MediaLibrary &mediaLibrary)
    : session_(session), mediaLibrary_(mediaLibrary)
{
}

bool TimelineEditingController::focusClip(int clipId, bool resetToBeginning)
{
    const TimelineClip *clip = session_.timelineModel().findClip(clipId);
    if (clip == nullptr)
        return false;

    const int assetIndex = assetIndexForMediaAsset(clip->mediaAssetId);
    if (assetIndex < 0)
        return false;

    session_.selectTimelineClip(clipId, assetIndex);
    synchronizePlaybackDuration(resetToBeginning);
    return true;
}

void TimelineEditingController::focusFrame(int frame)
{
    const ResolvedTimelineFrame resolved = TimelinePlaybackResolver::resolve(
        session_.timelineModel(), mediaLibrary_, frame);

    // V1 is the visible editing target when video and audio overlap. If V1 is
    // empty at this frame, allow the A1 placement to receive focus instead.
    if (resolved.video)
        focusClip(resolved.video->clipId, false);
    else if (resolved.audio)
        focusClip(resolved.audio->clipId, false);
    else {
        session_.focusTimeline();
        synchronizePlaybackDuration(false);
    }

    // Focus first so the seek is clamped against timeline duration rather
    // than the previously focused source asset's duration.
    session_.seekTimeline(frame);
}

void TimelineEditingController::followPlaybackFrame()
{
    // Playback already owns the current frame and duration. Unlike focusFrame,
    // this only updates visible selection and never resets playback state.
    const ResolvedTimelineFrame resolved = TimelinePlaybackResolver::resolve(
        session_.timelineModel(), mediaLibrary_, session_.playbackState().currentFrame);

    if (resolved.video) {
        const int assetIndex = assetIndexForMediaAsset(resolved.video->mediaAssetId);
        if (assetIndex >= 0)
            session_.selectTimelineClip(resolved.video->clipId, assetIndex);
        return;
    }

    if (resolved.audio) {
        const int assetIndex = assetIndexForMediaAsset(resolved.audio->mediaAssetId);
        if (assetIndex >= 0)
            session_.selectTimelineClip(resolved.audio->clipId, assetIndex);
        return;
    }

    session_.focusTimeline();
}

void TimelineEditingController::focusEmptyTimeline()
{
    session_.focusTimeline();
    synchronizePlaybackDuration(true);
}

void TimelineEditingController::selectSourceAsset(int assetIndex)
{
    session_.selectAsset(assetIndex);
    synchronizePlaybackDuration(true);
}

void TimelineEditingController::seekFocusedPreview(int frame)
{
    if (session_.isTimelineFocused())
        focusFrame(frame);
    else
        session_.seekTimeline(frame);
}

bool TimelineEditingController::insertMediaAsset(int mediaAssetId, int startFrame)
{
    const LibraryMediaAsset *asset = mediaLibrary_.findAsset(mediaAssetId);
    if (asset == nullptr)
        return false;

    const TimelineTrackType trackType = asset->kind == MediaKind::Audio
        ? TimelineTrackType::Audio : TimelineTrackType::Video;
    return finishInsertedClip(session_.addTimelineClip(
        mediaAssetId, trackType, startFrame, asset->timelineDurationFrames));
}

bool TimelineEditingController::deleteClip(int clipId)
{
    const bool removedFocusedClip = session_.selectedTimelineClipId() == clipId;
    if (!session_.removeTimelineClip(clipId))
        return false;
    synchronizePlaybackDuration(removedFocusedClip);
    return true;
}

bool TimelineEditingController::splitAtHead()
{
    if (!canSplitAtHead())
        return false;

    const TimelineClip *clip = session_.timelineModel().findClip(
        session_.selectedTimelineClipId());
    if (clip == nullptr)
        return false;
    const LibraryMediaAsset *asset = mediaLibrary_.findAsset(clip->mediaAssetId);
    if (asset == nullptr)
        return false;

    return finishInsertedClip(session_.splitTimelineClip(
        clip->id, session_.playbackState().currentFrame, asset->kind));
}

bool TimelineEditingController::copy()
{
    return session_.copySelectedTimelineClip();
}

bool TimelineEditingController::cut()
{
    if (!session_.cutSelectedTimelineClip())
        return false;
    synchronizePlaybackDuration(true);
    return true;
}

bool TimelineEditingController::paste()
{
    if (!canPaste())
        return false;
    return finishInsertedClip(session_.pasteTimelineClip(
        session_.playbackState().currentFrame));
}

bool TimelineEditingController::duplicate()
{
    return finishInsertedClip(session_.duplicateSelectedTimelineClip());
}

bool TimelineEditingController::canCopy() const
{
    return session_.timelineModel().findClip(
        session_.selectedTimelineClipId()) != nullptr;
}

bool TimelineEditingController::canCut() const
{
    return canCopy();
}

bool TimelineEditingController::canPaste() const
{
    return session_.hasTimelineClipboard()
        && mediaLibrary_.findAsset(
               session_.timelineClipboardMediaAssetId()) != nullptr;
}

bool TimelineEditingController::canDuplicate() const
{
    return canCopy();
}

bool TimelineEditingController::canSplitAtHead() const
{
    const TimelineClip *clip = session_.timelineModel().findClip(
        session_.selectedTimelineClipId());
    if (clip == nullptr)
        return false;

    const int splitFrame = session_.playbackState().currentFrame;
    return splitFrame > clip->state.startFrame
        && splitFrame < clip->state.startFrame + clip->state.durationFrames;
}

void TimelineEditingController::synchronizePlaybackDuration(
    bool resetToBeginning)
{
    if (session_.isTimelineFocused()) {
        session_.setPlaybackDuration(
            session_.timelineModel().contentDurationFrames(), resetToBeginning);
        return;
    }

    const int assetIndex = session_.selectedAssetIndex();
    const auto &assets = mediaLibrary_.assets();
    if (assetIndex >= 0 && assetIndex < static_cast<int>(assets.size())) {
        session_.setPlaybackDuration(
            assets[assetIndex].timelineDurationFrames, resetToBeginning);
    }
}

int TimelineEditingController::assetIndexForMediaAsset(int mediaAssetId) const
{
    const auto &assets = mediaLibrary_.assets();
    const auto asset = std::find_if(assets.begin(), assets.end(),
        [mediaAssetId](const LibraryMediaAsset &candidate) {
            return candidate.id == mediaAssetId;
        });
    return asset == assets.end() ? -1
        : static_cast<int>(asset - assets.begin());
}

bool TimelineEditingController::finishInsertedClip(int clipId)
{
    if (clipId == 0)
        return false;
    const TimelineClip *clip = session_.timelineModel().findClip(clipId);
    if (clip == nullptr)
        return false;
    const int clipStart = clip->state.startFrame;

    // Every user-facing insertion ends in the same state: the new placement
    // owns focus, Properties edits it, and the playhead shows its first frame.
    if (!focusClip(clipId, false))
        return false;
    session_.seekTimeline(clipStart);
    return true;
}
