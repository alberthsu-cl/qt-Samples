#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QQueue>
#include <QString>

#include <QMediaPlayer>
#include <QVideoSink>

class MediaLibrary;

// Qt presentation cache. Refresh performs file I/O before views paint; view
// delegates only read already-decoded QImage values from this cache.
class QtThumbnailCache final : public QObject
{
    Q_OBJECT

public:
    explicit QtThumbnailCache(QObject *parent = nullptr);

    void refresh(const MediaLibrary &mediaLibrary);
    QImage imageFor(int mediaAssetId) const;

signals:
    void thumbnailChanged(int mediaAssetId);

private:
    struct PendingVideo {
        int mediaAssetId = 0;
        QString filePath;
    };

    void startNextVideo();
    void finishCurrentVideo();

    QHash<int, QImage> images_;
    QQueue<PendingVideo> pendingVideos_;
    QMediaPlayer videoPlayer_;
    QVideoSink videoSink_;
    int currentVideoAssetId_ = 0;
    bool isAcceptingCurrentVideoFrames_ = false;
};
