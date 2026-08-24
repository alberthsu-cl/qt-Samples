#pragma once

#include "ProjectState.h"

#include <QWidget>

#include <functional>

class QtTimelineCanvas final : public QWidget
{
public:
    using SeekHandler = std::function<void(int frame)>;
    using TimelineClipEditedHandler = std::function<void(const TimelineClipState &state)>;

    explicit QtTimelineCanvas(QWidget *parent = nullptr);

    void setSelectedAssetIndex(int selectedAssetIndex);
    void setClipSettings(const ClipSettings &settings);
    void setTimelineClipState(const TimelineClipState &state);
    void setPlaybackState(const PlaybackState &state);
    void setViewState(const TimelineViewState &state);
    void setSeekHandler(SeekHandler handler);
    void setTimelineClipEditedHandler(TimelineClipEditedHandler handler);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int frameAtRulerX(int x) const;
    int frameAtTimelineX(int x) const;
    QRect timelineClipRect() const;

    int selectedAssetIndex_ = 0;
    ClipSettings clipSettings_;
    TimelineClipState timelineClipState_;
    PlaybackState playbackState_;
    TimelineViewState viewState_;
    SeekHandler seekHandler_;
    TimelineClipEditedHandler timelineClipEditedHandler_;
    bool isDraggingClip_ = false;
    int dragFrameOffset_ = 0;
    TimelineClipState dragPreviewState_;
};
