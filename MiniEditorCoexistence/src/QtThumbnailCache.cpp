#include "QtThumbnailCache.h"

#include "MediaLibrary.h"
#include "QtFrameEffectProcessor.h"

#include <QImageReader>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>

#include <QSet>

#include <utility>

QtThumbnailCache::QtThumbnailCache(QObject *parent)
    : QObject(parent)
{
    videoPlayer_.setVideoOutput(&videoSink_);

    connect(&videoSink_, &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) {
                if (currentVideoAssetId_ == 0 || !isAcceptingCurrentVideoFrames_
                    || !frame.isValid()) {
                    return;
                }

                const QImage image = frame.toImage();
                if (image.isNull())
                    return;

                const int completedAssetId = currentVideoAssetId_;
                images_.insert(completedAssetId, image);
                finishCurrentVideo();
                emit thumbnailChanged(completedAssetId);
            });

    connect(&videoPlayer_, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::LoadedMedia && currentVideoAssetId_ != 0) {
                    // Frames are accepted only after this specific source has
                    // finished loading. Buffered callbacks from the previous
                    // source arrive while this flag is false and are ignored.
                    isAcceptingCurrentVideoFrames_ = true;
                    videoPlayer_.setPosition(0);
                    videoPlayer_.play();
                } else if (status == QMediaPlayer::InvalidMedia
                           && currentVideoAssetId_ != 0) {
                    finishCurrentVideo();
                }
            });
}

void QtThumbnailCache::refresh(const MediaLibrary &mediaLibrary)
{
    videoPlayer_.stop();
    videoPlayer_.setSource({});
    currentVideoAssetId_ = 0;
    isAcceptingCurrentVideoFrames_ = false;
    pendingVideos_.clear();
    images_.clear();
    timelineImages_.clear();
    for (const LibraryMediaAsset &asset : mediaLibrary.assets()) {
        if (asset.filePath.empty())
            continue;

        const QString filePath = QString::fromStdWString(asset.filePath.wstring());
        if (asset.kind == MediaKind::Image) {
            QImageReader reader(filePath);
            const QImage image = reader.read();
            if (!image.isNull())
                images_.insert(asset.id, image);
        } else if (asset.kind == MediaKind::Video) {
            pendingVideos_.enqueue({ asset.id, filePath });
        }
    }
    startNextVideo();
}

QImage QtThumbnailCache::imageFor(int mediaAssetId) const
{
    return images_.value(mediaAssetId);
}

void QtThumbnailCache::prepareTimelineThumbnails(
    const std::vector<TimelineClip> &clips)
{
    QSet<int> activeClipIds;
    for (const TimelineClip &clip : clips) {
        activeClipIds.insert(clip.id);
        const QImage source = images_.value(clip.mediaAssetId);
        const bool needsEffect = clip.settings.effect != ClipEffectKind::None
            && clip.settings.effectIntensityPercent > 0;
        if (source.isNull() || !needsEffect) {
            timelineImages_.remove(clip.id);
            continue;
        }

        const auto existing = timelineImages_.constFind(clip.id);
        if (existing != timelineImages_.constEnd()
            && existing->mediaAssetId == clip.mediaAssetId
            && existing->effect == clip.settings.effect
            && existing->intensityPercent
                == clip.settings.effectIntensityPercent
            && existing->sourceCacheKey == source.cacheKey()) {
            continue;
        }

        // Timeline tiles are small. Apply the effect to a bounded image here,
        // during state refresh, so paintEvent remains a cache-only operation.
        const QImage scaledSource = source.scaled(
            192, 108, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        TimelineThumbnailEntry entry;
        entry.mediaAssetId = clip.mediaAssetId;
        entry.effect = clip.settings.effect;
        entry.intensityPercent = clip.settings.effectIntensityPercent;
        entry.sourceCacheKey = source.cacheKey();
        entry.image = QtFrameEffectProcessor::applyEffect(
            scaledSource, entry.effect, entry.intensityPercent);
        timelineImages_.insert(clip.id, std::move(entry));
    }

    for (auto iterator = timelineImages_.begin();
         iterator != timelineImages_.end();) {
        if (!activeClipIds.contains(iterator.key()))
            iterator = timelineImages_.erase(iterator);
        else
            ++iterator;
    }
}

QImage QtThumbnailCache::timelineImageFor(const TimelineClip &clip) const
{
    const auto entry = timelineImages_.constFind(clip.id);
    return entry == timelineImages_.constEnd()
        ? imageFor(clip.mediaAssetId) : entry->image;
}

void QtThumbnailCache::startNextVideo()
{
    if (currentVideoAssetId_ != 0 || pendingVideos_.isEmpty())
        return;

    const PendingVideo request = pendingVideos_.dequeue();
    currentVideoAssetId_ = request.mediaAssetId;
    isAcceptingCurrentVideoFrames_ = false;
    videoPlayer_.setSource(QUrl::fromLocalFile(request.filePath));
}

void QtThumbnailCache::finishCurrentVideo()
{
    isAcceptingCurrentVideoFrames_ = false;
    videoPlayer_.stop();
    videoPlayer_.setSource({});
    videoSink_.setVideoFrame({});
    currentVideoAssetId_ = 0;
    // Let queued sink callbacks from the old decoder drain while there is no
    // asset ID to receive them. Starting synchronously can assign an old
    // buffered frame to the next queued video.
    QTimer::singleShot(50, this, [this] { startNextVideo(); });
}
