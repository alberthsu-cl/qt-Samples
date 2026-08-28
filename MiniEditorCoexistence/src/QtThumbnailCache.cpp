#include "QtThumbnailCache.h"

#include "MediaLibrary.h"

#include <QImageReader>
#include <QUrl>
#include <QVideoFrame>

QtThumbnailCache::QtThumbnailCache(QObject *parent)
    : QObject(parent)
{
    videoPlayer_.setVideoOutput(&videoSink_);

    connect(&videoSink_, &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) {
                if (currentVideoAssetId_ == 0 || !frame.isValid())
                    return;

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
    videoPlayer_.setSource(QUrl::fromLocalFile(request.filePath));
}

void QtThumbnailCache::finishCurrentVideo()
{
    videoPlayer_.stop();
    videoPlayer_.setSource({});
    currentVideoAssetId_ = 0;
    startNextVideo();
}
