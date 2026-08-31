#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QQueue>
#include <QString>

#include <QMediaPlayer>
#include <QVideoSink>

#include "TimelineModel.h"

#include <vector>

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
    // Builds small effect-aware images before the timeline paints. Entries are
    // keyed per placement, so changing one clip never changes the source image
    // used by Media Library or another placement of the same asset.
    void prepareTimelineThumbnails(const std::vector<TimelineClip> &clips);
    QImage timelineImageFor(const TimelineClip &clip) const;

signals:
    void thumbnailChanged(int mediaAssetId);

private:
    struct PendingVideo {
        int mediaAssetId = 0;
        QString filePath;
    };

    struct TimelineThumbnailEntry {
        int mediaAssetId = 0;
        ClipEffectKind effect = ClipEffectKind::None;
        int intensityPercent = 100;
        quint64 sourceCacheKey = 0;
        QImage image;
    };

    void startNextVideo();
    void finishCurrentVideo();

    QHash<int, QImage> images_;
    QHash<int, TimelineThumbnailEntry> timelineImages_;
    QQueue<PendingVideo> pendingVideos_;
    QMediaPlayer videoPlayer_;
    QVideoSink videoSink_;
    int currentVideoAssetId_ = 0;
    bool isAcceptingCurrentVideoFrames_ = false;
};
