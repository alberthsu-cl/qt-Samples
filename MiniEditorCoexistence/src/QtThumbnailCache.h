#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>

#include <QMediaPlayer>
#include <QVideoSink>

#include "TimelineModel.h"

#include <optional>
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
    // The canvas asks for the closest decoded sample to its current source
    // frame. That lets a clip keep meaningful visual variety as zoom changes
    // without decoding again during paintEvent().
    QImage timelineImageFor(const TimelineClip &clip, int sourceFrame) const;
    QImage timelineImageFor(const TimelineClip &clip) const;

signals:
    void thumbnailChanged(int mediaAssetId);

private:
    enum class VideoDecodeTarget {
        Library,
        TimelineClip
    };

    struct PendingVideo {
        int mediaAssetId = 0;
        int timelineClipId = 0;
        int sourceFrame = 0;
        QString filePath;
        VideoDecodeTarget target = VideoDecodeTarget::Library;
    };

    struct TimelineThumbnailSample {
        int sourceFrame = 0;
        QImage sourceImage;
        QImage image;
    };

    struct TimelineThumbnailEntry {
        int mediaAssetId = 0;
        ClipEffectKind effect = ClipEffectKind::None;
        int intensityPercent = 100;
        int sourceInFrame = 0;
        int durationFrames = 0;
        std::vector<TimelineThumbnailSample> samples;
    };

    void startNextVideo();
    void finishCurrentVideo();
    void queueTimelineVideoSample(const TimelineClip &clip, int sourceFrame);
    void storeTimelineVideoSample(int clipId, int sourceFrame,
                                  const QImage &sourceImage);
    void applyEffectToTimelineEntry(TimelineThumbnailEntry &entry);
    QString videoRequestKey(int clipId, int sourceFrame) const;

    QHash<int, QImage> images_;
    QHash<int, TimelineThumbnailEntry> timelineImages_;
    QHash<int, QString> imageFilePaths_;
    QHash<int, QString> videoFilePaths_;
    QQueue<PendingVideo> pendingVideos_;
    QSet<QString> queuedVideoRequestKeys_;
    QMediaPlayer videoPlayer_;
    QVideoSink videoSink_;
    std::optional<PendingVideo> currentVideo_;
    bool isAcceptingCurrentVideoFrames_ = false;
};
