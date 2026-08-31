#include "QtThumbnailCache.h"

#include "MediaLibrary.h"
#include "QtFrameEffectProcessor.h"
#include "ThumbnailRequestModel.h"

#include <QImageReader>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>

#include <utility>

QtThumbnailCache::QtThumbnailCache(QObject *parent)
    : QObject(parent)
{
    videoPlayer_.setVideoOutput(&videoSink_);

    connect(&videoSink_, &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) {
                if (!currentVideo_ || !isAcceptingCurrentVideoFrames_
                    || !frame.isValid()) {
                    return;
                }

                const QImage image = frame.toImage();
                if (image.isNull())
                    return;

                const PendingVideo completed = *currentVideo_;
                if (completed.target == VideoDecodeTarget::Library) {
                    images_.insert(completed.mediaAssetId, image);
                } else {
                    storeTimelineVideoSample(completed.timelineClipId,
                                             completed.sourceFrame, image);
                }
                finishCurrentVideo();
                emit thumbnailChanged(completed.mediaAssetId);
            });

    connect(&videoPlayer_, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::LoadedMedia && currentVideo_) {
                    // Frames are accepted only after this specific source has
                    // finished loading. Buffered callbacks from the previous
                    // source arrive while this flag is false and are ignored.
                    isAcceptingCurrentVideoFrames_ = true;
                    constexpr qint64 kMillisecondsPerSecond = 1000;
                    constexpr qint64 kFramesPerSecond = 30;
                    videoPlayer_.setPosition(
                        static_cast<qint64>(currentVideo_->sourceFrame)
                            * kMillisecondsPerSecond / kFramesPerSecond);
                    videoPlayer_.play();
                } else if (status == QMediaPlayer::InvalidMedia
                           && currentVideo_) {
                    finishCurrentVideo();
                }
            });
}

void QtThumbnailCache::refresh(const MediaLibrary &mediaLibrary)
{
    QHash<int, QString> requestedImagePaths;
    QHash<int, QString> requestedVideoPaths;
    for (const LibraryMediaAsset &asset : mediaLibrary.assets()) {
        if (asset.filePath.empty())
            continue;

        const QString filePath = QString::fromStdWString(asset.filePath.wstring());
        if (asset.kind == MediaKind::Image)
            requestedImagePaths.insert(asset.id, filePath);
        else if (asset.kind == MediaKind::Video)
            requestedVideoPaths.insert(asset.id, filePath);
    }

    const auto stillContainsEveryExistingPath = [](
        const QHash<int, QString> &current,
        const QHash<int, QString> &requested) {
        for (auto iterator = current.cbegin(); iterator != current.cend(); ++iterator) {
            if (requested.value(iterator.key()) != iterator.value())
                return false;
        }
        return true;
    };
    const bool hasReplacedOrRemovedAsset =
        !stillContainsEveryExistingPath(imageFilePaths_, requestedImagePaths)
        || !stillContainsEveryExistingPath(videoFilePaths_, requestedVideoPaths);

    if (hasReplacedOrRemovedAsset) {
        // Loading another project or removing/replacing a file can make IDs
        // point at different sources. A full reset is correct in that case.
        videoPlayer_.stop();
        videoPlayer_.setSource({});
        currentVideo_.reset();
        isAcceptingCurrentVideoFrames_ = false;
        pendingVideos_.clear();
        queuedVideoRequestKeys_.clear();
        images_.clear();
        timelineImages_.clear();
        imageFilePaths_.clear();
        videoFilePaths_.clear();
    }

    // Importing only adds entries to the requested maps. Leave existing
    // source and timeline-strip images intact, then enqueue only the new work.
    for (auto iterator = requestedImagePaths.cbegin();
         iterator != requestedImagePaths.cend(); ++iterator) {
        if (imageFilePaths_.contains(iterator.key()))
            continue;
        QImageReader reader(iterator.value());
        const QImage image = reader.read();
        if (!image.isNull())
            images_.insert(iterator.key(), image);
        imageFilePaths_.insert(iterator.key(), iterator.value());
    }
    for (auto iterator = requestedVideoPaths.cbegin();
         iterator != requestedVideoPaths.cend(); ++iterator) {
        if (videoFilePaths_.contains(iterator.key()))
            continue;
        videoFilePaths_.insert(iterator.key(), iterator.value());
        pendingVideos_.enqueue({ iterator.key(), 0, 0, iterator.value(),
                                 VideoDecodeTarget::Library });
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
        // The cache only stores file paths, but an image entry can be found
        // through its decoded source. Video entries are queued below.
        const bool isVideo = videoFilePaths_.contains(clip.mediaAssetId);
        const QImage sourceImage = images_.value(clip.mediaAssetId);
        if (!isVideo && sourceImage.isNull()) {
            timelineImages_.remove(clip.id);
            continue;
        }

        const auto existing = timelineImages_.constFind(clip.id);
        const bool sameSourceRange = existing != timelineImages_.constEnd()
            && existing->mediaAssetId == clip.mediaAssetId
            && existing->sourceInFrame == clip.state.sourceInFrame
            && existing->durationFrames == clip.state.durationFrames;
        if (!sameSourceRange) {
            TimelineThumbnailEntry entry;
            entry.mediaAssetId = clip.mediaAssetId;
            entry.sourceInFrame = clip.state.sourceInFrame;
            entry.durationFrames = clip.state.durationFrames;
            const MediaKind kind = isVideo ? MediaKind::Video : MediaKind::Image;
            const std::vector<ThumbnailRequest> requests =
                ThumbnailRequestModel::timelineStrip(clip, kind, 8 * 96);
            for (const ThumbnailRequest &request : requests) {
                entry.samples.push_back({ request.sourceFrame, {}, {} });
            }
            timelineImages_.insert(clip.id, std::move(entry));
        }

        TimelineThumbnailEntry &entry = timelineImages_[clip.id];
        const bool effectChanged = entry.effect != clip.settings.effect
            || entry.intensityPercent != clip.settings.effectIntensityPercent;
        entry.effect = clip.settings.effect;
        entry.intensityPercent = clip.settings.effectIntensityPercent;

        if (isVideo) {
            for (const TimelineThumbnailSample &sample : entry.samples) {
                if (sample.sourceImage.isNull())
                    queueTimelineVideoSample(clip, sample.sourceFrame);
            }
        } else {
            for (TimelineThumbnailSample &sample : entry.samples)
                sample.sourceImage = sourceImage;
        }
        if (effectChanged || !isVideo)
            applyEffectToTimelineEntry(entry);
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
    return timelineImageFor(clip, clip.state.sourceInFrame);
}

QImage QtThumbnailCache::timelineImageFor(const TimelineClip &clip,
                                          int sourceFrame) const
{
    const auto entry = timelineImages_.constFind(clip.id);
    if (entry == timelineImages_.constEnd() || entry->samples.empty())
        return imageFor(clip.mediaAssetId);

    const TimelineThumbnailSample *closest = nullptr;
    for (const TimelineThumbnailSample &sample : entry->samples) {
        if (sample.image.isNull())
            continue;
        if (closest == nullptr || std::abs(sample.sourceFrame - sourceFrame)
            < std::abs(closest->sourceFrame - sourceFrame)) {
            closest = &sample;
        }
    }
    return closest == nullptr ? imageFor(clip.mediaAssetId) : closest->image;
}

void QtThumbnailCache::startNextVideo()
{
    if (currentVideo_ || pendingVideos_.isEmpty())
        return;

    const PendingVideo request = pendingVideos_.dequeue();
    currentVideo_ = request;
    isAcceptingCurrentVideoFrames_ = false;
    videoPlayer_.setSource(QUrl::fromLocalFile(request.filePath));
}

void QtThumbnailCache::finishCurrentVideo()
{
    isAcceptingCurrentVideoFrames_ = false;
    videoPlayer_.stop();
    videoPlayer_.setSource({});
    videoSink_.setVideoFrame({});
    if (currentVideo_ && currentVideo_->target == VideoDecodeTarget::TimelineClip) {
        queuedVideoRequestKeys_.remove(videoRequestKey(
            currentVideo_->timelineClipId, currentVideo_->sourceFrame));
    }
    currentVideo_.reset();
    // Let queued sink callbacks from the old decoder drain while there is no
    // asset ID to receive them. Starting synchronously can assign an old
    // buffered frame to the next queued video.
    QTimer::singleShot(50, this, [this] { startNextVideo(); });
}

void QtThumbnailCache::queueTimelineVideoSample(const TimelineClip &clip,
                                                 int sourceFrame)
{
    const QString filePath = videoFilePaths_.value(clip.mediaAssetId);
    if (filePath.isEmpty())
        return;

    const QString key = videoRequestKey(clip.id, sourceFrame);
    if (queuedVideoRequestKeys_.contains(key))
        return;

    queuedVideoRequestKeys_.insert(key);
    pendingVideos_.enqueue({ clip.mediaAssetId, clip.id, sourceFrame, filePath,
                             VideoDecodeTarget::TimelineClip });
    startNextVideo();
}

void QtThumbnailCache::storeTimelineVideoSample(int clipId, int sourceFrame,
                                                 const QImage &sourceImage)
{
    auto entry = timelineImages_.find(clipId);
    if (entry == timelineImages_.end())
        return;

    for (TimelineThumbnailSample &sample : entry->samples) {
        if (sample.sourceFrame != sourceFrame)
            continue;
        sample.sourceImage = sourceImage;
        const QImage scaled = sourceImage.scaled(
            192, 108, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        sample.image = QtFrameEffectProcessor::applyEffect(
            scaled, entry->effect, entry->intensityPercent);
        return;
    }
}

void QtThumbnailCache::applyEffectToTimelineEntry(TimelineThumbnailEntry &entry)
{
    for (TimelineThumbnailSample &sample : entry.samples) {
        if (sample.sourceImage.isNull())
            continue;
        const QImage scaled = sample.sourceImage.scaled(
            192, 108, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        sample.image = QtFrameEffectProcessor::applyEffect(
            scaled, entry.effect, entry.intensityPercent);
    }
}

QString QtThumbnailCache::videoRequestKey(int clipId, int sourceFrame) const
{
    return QStringLiteral("%1:%2").arg(clipId).arg(sourceFrame);
}
