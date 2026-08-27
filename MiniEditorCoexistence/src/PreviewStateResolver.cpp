#include "PreviewStateResolver.h"

#include "ClipFade.h"
#include "EditorSession.h"
#include "MediaLibrary.h"
#include "TimelinePlaybackResolver.h"

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
    const TimelineClip *selectedClip = timeline.findClip(
        session.selectedTimelineClipId());
    const ResolvedTimelineFrame resolvedFrame = TimelinePlaybackResolver::resolve(
        timeline, mediaLibrary, playback.currentFrame);
    std::optional<ResolvedTimelineMedia> visibleVideo = resolvedFrame.video;
    bool applyVideoFade = true;

    // While stopped, Properties is an editor for the focused placement. Show
    // that video clip directly so opacity, scale, and position changes appear
    // immediately even when the timeline playhead is currently elsewhere.
    if (!playback.isPlaying && !playback.isPaused && selectedClip != nullptr
        && selectedClip->trackType == TimelineTrackType::Video) {
        visibleVideo = TimelinePlaybackResolver::resolveClip(
            *selectedClip, mediaLibrary, selectedClip->state.startFrame);
        // This is the clip as an edit target, not a rendered timeline frame.
        // Its fade-in would otherwise hide the very placement being adjusted.
        applyVideoFade = false;
    }

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
