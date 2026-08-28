#include "QtThumbnailCache.h"

#include "MediaLibrary.h"

#include <QImageReader>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>

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
