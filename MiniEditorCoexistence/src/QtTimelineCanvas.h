#pragma once

#include "ProjectState.h"
#include "TimelineModel.h"

#include <QWidget>

#include <functional>
#include <vector>

class QtTimelineCanvas final : public QWidget
{
public:
    using SeekHandler = std::function<void(int frame)>;
    using TimelineClipEditedHandler = std::function<void(int clipId,
                                                         const TimelineClipState &state)>;
    using MediaAssetDroppedHandler = std::function<void(int mediaAssetId, int frame)>;
    using AssetPresentationResolver = std::function<bool(int mediaAssetId,
                                                          QString *displayName,
                                                          QColor *color)>;
    using TimelineClipDeletedHandler = std::function<void(int clipId)>;

    explicit QtTimelineCanvas(QWidget *parent = nullptr);

    void setSelectedAssetIndex(int selectedAssetIndex);
    void setClipSettings(const ClipSettings &settings);
    void setTimelineClipState(const TimelineClipState &state);
    void setPlaybackState(const PlaybackState &state);
    void setViewState(const TimelineViewState &state);
    void setSeekHandler(SeekHandler handler);
    void setTimelineClipEditedHandler(TimelineClipEditedHandler handler);
    void setTimelineClips(const std::vector<TimelineClip> &clips);
    void setTimelineDuration(int durationFrames);
    void setMediaAssetDroppedHandler(MediaAssetDroppedHandler handler);
    void setAssetPresentationResolver(AssetPresentationResolver resolver);
    void setTimelineClipDeletedHandler(TimelineClipDeletedHandler handler);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    int frameAtRulerX(int x) const;
    int frameAtTimelineX(int x) const;
    QRect timelineClipRect(const TimelineClip &clip) const;
    const TimelineClip *clipAt(const QPoint &point) const;

    int selectedAssetIndex_ = 0;
    ClipSettings clipSettings_;
    TimelineClipState timelineClipState_;
    PlaybackState playbackState_;
    TimelineViewState viewState_;
    SeekHandler seekHandler_;
    TimelineClipEditedHandler timelineClipEditedHandler_;
    MediaAssetDroppedHandler mediaAssetDroppedHandler_;
    AssetPresentationResolver assetPresentationResolver_;
    TimelineClipDeletedHandler timelineClipDeletedHandler_;
    std::vector<TimelineClip> timelineClips_;
    int timelineDurationFrames_ = 600;
    bool isDraggingClip_ = false;
    int dragFrameOffset_ = 0;
    int dragClipId_ = 0;
    int selectedClipId_ = 0;
    TimelineClipState dragPreviewState_;
};
