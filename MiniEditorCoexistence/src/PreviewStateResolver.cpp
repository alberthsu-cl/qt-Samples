#include "PreviewStateResolver.h"

#include "ClipFade.h"
#include "EditorSession.h"
#include "MediaLibrary.h"
#include "TimelinePlaybackResolver.h"

std::optional<ResolvedTimelineMedia> PreviewStateResolver::resolveTimelineVideo(
    const EditorSession &session, const MediaLibrary &mediaLibrary)
{
    if (!session.isTimelineFocused())
        return std::nullopt;

    const PlaybackState &playback = session.timelinePlaybackState();
    const TimelineModel &timeline = session.timelineModel();
    const TimelineClip *selectedClip = timeline.findClip(
        session.selectedTimelineClipId());

    const bool playheadIsInsideSelectedClip = selectedClip != nullptr
        && playback.currentFrame >= selectedClip->state.startFrame
        && playback.currentFrame < selectedClip->state.startFrame
                                      + selectedClip->state.durationFrames;

    // Clicking a clip while stopped previews that edit target even when the
    // head is elsewhere. Once the user moves the head into the selected clip,
    // however, the head becomes the preview target and must resolve its exact
    // source frame.
    if (!playback.isPlaying && !playback.isPaused && selectedClip != nullptr
        && selectedClip->trackType == TimelineTrackType::Video
        && !playheadIsInsideSelectedClip) {
        return TimelinePlaybackResolver::resolveClip(
            *selectedClip, mediaLibrary, selectedClip->state.startFrame);
    }

    return TimelinePlaybackResolver::resolve(
        timeline, mediaLibrary, playback.currentFrame).video;
}

PreviewState PreviewStateResolver::resolve(const EditorSession &session,
                                           const MediaLibrary &mediaLibrary)
{
    PreviewState preview;
    const PlaybackState &playback = session.playbackState();
    const bool timelineMode = session.isTimelineFocused();
    preview.mode = timelineMode ? PreviewMode::Timeline : PreviewMode::Source;

    if (!timelineMode) {
        const int assetIndex = session.selectedAssetIndex();
        const auto &assets = mediaLibrary.assets();
        if (assetIndex < 0 || assetIndex >= static_cast<int>(assets.size()))
            return preview;

        const LibraryMediaAsset &asset = assets[assetIndex];
        preview.hasMedia = true;
        preview.mediaAssetId = asset.id;
        preview.displayName = asset.displayName;
        preview.thumbnailColorRgb = asset.thumbnailColorRgb;
        preview.settings = {};
        preview.effectiveOpacityPercent = preview.settings.opacityPercent;
        preview.mediaKind = asset.kind;
        preview.sourceFrame = asset.kind == MediaKind::Image
            ? 0 : playback.currentFrame;
        preview.sourceDurationFrames = asset.timelineDurationFrames;
        return preview;
    }

    const TimelineModel &timeline = session.timelineModel();
    const ResolvedTimelineFrame resolvedFrame = TimelinePlaybackResolver::resolve(
        timeline, mediaLibrary, playback.currentFrame);
    const std::optional<ResolvedTimelineMedia> visibleVideo =
        resolveTimelineVideo(session, mediaLibrary);
    // A stopped focused placement is an edit target. Its fade-in must not hide
    // the item being adjusted; playback/paused frames still evaluate fades.
    const bool isFocusedEditTarget = !playback.isPlaying && !playback.isPaused
        && visibleVideo
        && visibleVideo->clipId == session.selectedTimelineClipId();
    const bool applyVideoFade = !isFocusedEditTarget;

    preview.timelineFrame = playback.currentFrame;
    if (resolvedFrame.audio) {
        const LibraryMediaAsset *audioAsset = mediaLibrary.findAsset(
            resolvedFrame.audio->mediaAssetId);
        if (audioAsset != nullptr) {
            preview.hasAudio = true;
            preview.audioDisplayName = audioAsset->displayName;
            preview.audioSourceFrame = resolvedFrame.audio->sourceFrame;
            preview.audioSourceDurationFrames = resolvedFrame.audio->sourceDurationFrames;
            preview.audioFadeGainPercent = resolvedFrame.audio->fadeGainPercent;
        }
    }

    if (!visibleVideo)
        return preview;

    const LibraryMediaAsset *asset = mediaLibrary.findAsset(visibleVideo->mediaAssetId);
    if (asset == nullptr)
        return preview;

    preview.hasMedia = true;
    preview.mediaAssetId = asset->id;
    preview.displayName = asset->displayName;
    preview.thumbnailColorRgb = asset->thumbnailColorRgb;
    preview.settings = visibleVideo->settings;
    preview.mediaKind = visibleVideo->mediaKind;
    preview.clipLocalFrame = visibleVideo->clipLocalFrame;
    preview.sourceFrame = visibleVideo->sourceFrame;
    preview.sourceDurationFrames = visibleVideo->sourceDurationFrames;
    preview.videoFadeGainPercent = applyVideoFade
        ? visibleVideo->fadeGainPercent : 100;
    preview.effectiveOpacityPercent =
        preview.settings.opacityPercent * preview.videoFadeGainPercent / 100;
    return preview;
}
