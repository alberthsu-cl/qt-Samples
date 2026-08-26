#pragma once

#include "ProjectState.h"
#include "TimelineAssetPresentation.h"
#include "TimelineClipEdit.h"
#include "TimelineGeometry.h"
#include "TimelineModel.h"

#include <QWidget>

#include <functional>
#include <optional>
#include <vector>

class QMimeData;

class QtTimelineCanvas final : public QWidget
{
public:
    using SeekHandler = std::function<void(int frame)>;
    using TimelineClipEditedHandler = std::function<void(int clipId,
                                                         const TimelineClipState &state)>;
    using MediaAssetDroppedHandler = std::function<void(int mediaAssetId, int frame)>;
    using AssetPresentationResolver =
        std::function<std::optional<TimelineAssetPresentation>(int mediaAssetId)>;
    using TimelineClipDeletedHandler = std::function<void(int clipId)>;
    using TimelineClipSelectedHandler = std::function<void(int clipId)>;

    explicit QtTimelineCanvas(QWidget *parent = nullptr);

    void setSelectedAssetIndex(int selectedAssetIndex);
    void setClipSettings(const ClipSettings &settings);
    void setTimelineClipState(const TimelineClipState &state);
    void setPlaybackState(const PlaybackState &state);
    void setViewState(const TimelineViewState &state);
    void setSeekHandler(SeekHandler handler);
    void setTimelineClipEditedHandler(TimelineClipEditedHandler handler);
    void setTimelineClips(const std::vector<TimelineClip> &clips);
    void setSelectedClipId(int clipId);
    void setTimelineDuration(int durationFrames);
    void setMediaAssetDroppedHandler(MediaAssetDroppedHandler handler);
    void setAssetPresentationResolver(AssetPresentationResolver resolver);
    void setTimelineClipDeletedHandler(TimelineClipDeletedHandler handler);
    void setTimelineClipSelectedHandler(TimelineClipSelectedHandler handler);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    TimelineGeometry geometry() const;
    bool isEditingClip() const;
    void updateDragPreview(int timelineX);
    void updateMouseCursor(const QPoint &point);
    bool updateMediaDropPreview(const QMimeData *mimeData, int timelineX);
    void clearMediaDropPreview();

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
    TimelineClipSelectedHandler timelineClipSelectedHandler_;
    std::vector<TimelineClip> timelineClips_;
    int timelineDurationFrames_ = 600;
    TimelineClipHitRegion dragRegion_ = TimelineClipHitRegion::None;
    int dragFrameOffset_ = 0;
    int dragClipId_ = 0;
    int selectedClipId_ = 0;
    TimelineClipState dragOriginalState_;
    TimelineClipState dragPreviewState_;
    bool isMediaDropPreviewVisible_ = false;
    int mediaDropAssetId_ = 0;
    int mediaDropStartFrame_ = 0;
    TimelineAssetPresentation mediaDropPresentation_;
};
