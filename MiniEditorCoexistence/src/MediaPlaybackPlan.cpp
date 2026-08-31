#include "MediaPlaybackPlan.h"

#include "EditorSession.h"
#include "MediaLibrary.h"
#include "PreviewStateResolver.h"

bool MediaPlaybackPlan::hasMedia() const
{
    return context != MediaPlaybackContext::None && mediaAssetId != 0;
}

bool MediaPlaybackPlan::usesMediaDecoder() const
{
    return hasMedia() && mediaKind != MediaKind::Image;
}

bool MediaPlaybackPlan::needsSilentVideoPreroll() const
{
    return hasMedia() && mediaKind == MediaKind::Video && !shouldPlay;
}

MediaPlaybackPlan MediaPlaybackPlanResolver::resolve(
    const EditorSession &session, const MediaLibrary &mediaLibrary)
{
    const PlaybackState &playback = session.playbackState();

    if (!session.isTimelineFocused()) {
        const auto &assets = mediaLibrary.assets();
        const int assetIndex = session.selectedAssetIndex();
        if (assetIndex < 0 || assetIndex >= static_cast<int>(assets.size()))
            return {};

        const LibraryMediaAsset &asset = assets[assetIndex];
        return MediaPlaybackPlan{
            MediaPlaybackContext::Source,
            asset.id,
            0,
            asset.kind,
            asset.kind == MediaKind::Image ? 0 : playback.currentFrame,
            asset.timelineDurationFrames,
            playback.isPlaying,
            playback.isPaused
        };
    }

    const std::optional<ResolvedTimelineMedia> media =
        PreviewStateResolver::resolveTimelineVideo(session, mediaLibrary);
    if (!media)
        return {};

    return MediaPlaybackPlan{
        MediaPlaybackContext::Timeline,
        media->mediaAssetId,
        media->clipId,
        media->mediaKind,
        media->sourceFrame,
        media->sourceDurationFrames,
        playback.isPlaying,
        playback.isPaused
    };
}
