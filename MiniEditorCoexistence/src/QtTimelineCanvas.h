#pragma once

#include "ProjectState.h"
#include "TimelineAssetPresentation.h"
#include "TimelineClipEdit.h"
#include "TimelineGeometry.h"
#include "TimelineModel.h"
#include "TimelinePresentationStateResolver.h"

#include <QWidget>

#include <functional>
#include <optional>
#include <vector>

class QMimeData;
class QResizeEvent;
class QToolButton;

class QtTimelineCanvas final : public QWidget
{
public:
    using SeekHandler = std::function<void(int frame)>;
    using TimelineClipEditedHandler = std::function<void(int clipId,
                                                         const TimelineClipState &state,
                                                         TimelineClipEditKind editKind)>;
    using MediaAssetDroppedHandler = std::function<void(int mediaAssetId, int frame)>;
    using AssetPresentationResolver =
        std::function<std::optional<TimelineAssetPresentation>(int mediaAssetId)>;
    using ClipThumbnailResolver = std::function<QImage(const TimelineClip &clip,
                                                        int sourceFrame)>;
    using TimelineClipDeletedHandler = std::function<void(int clipId)>;
    using TimelineClipSelectedHandler = std::function<void(int clipId)>;
    using TimelineFocusRequestedHandler = std::function<void()>;
    using AudioTrackVisibilityHandler = std::function<void(bool isVisible)>;

    explicit QtTimelineCanvas(QWidget *parent = nullptr);

    void setPresentationState(const TimelinePresentationState &state);
    // Focused setters remain useful to interaction tests. Production view
    // refreshes use setPresentationState() to avoid mixed timeline state.
    void setPlaybackState(const PlaybackState &state);
    void setViewState(const TimelineViewState &state);
    void setSeekHandler(SeekHandler handler);
    void setTimelineClipEditedHandler(TimelineClipEditedHandler handler);
    void setTimelineClips(const std::vector<TimelineClip> &clips);
    void setSelectedClipId(int clipId);
    void setTimelineDuration(int durationFrames);
    void setMediaAssetDroppedHandler(MediaAssetDroppedHandler handler);
    void setAssetPresentationResolver(AssetPresentationResolver resolver);
    void setClipThumbnailResolver(ClipThumbnailResolver resolver);
    void setTimelineClipDeletedHandler(TimelineClipDeletedHandler handler);
    void setTimelineClipSelectedHandler(TimelineClipSelectedHandler handler);
    void setTimelineFocusRequestedHandler(TimelineFocusRequestedHandler handler);
    void setAudioTrackVisibilityHandler(AudioTrackVisibilityHandler handler);

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
    void resizeEvent(QResizeEvent *event) override;

private:
    TimelineGeometry geometry() const;
    bool isEditingClip() const;
    void updateDragPreview(int timelineX);
    void updateMouseCursor(const QPoint &point);
    void layoutTrackHeaderControls();
    void updateAudioTrackVisibilityButton();
    bool updateMediaDropPreview(const QMimeData *mimeData, int timelineX);
    void clearMediaDropPreview();

    PlaybackState playbackState_;
    TimelineViewState viewState_;
    SeekHandler seekHandler_;
    TimelineClipEditedHandler timelineClipEditedHandler_;
    MediaAssetDroppedHandler mediaAssetDroppedHandler_;
    AssetPresentationResolver assetPresentationResolver_;
    ClipThumbnailResolver clipThumbnailResolver_;
    TimelineClipDeletedHandler timelineClipDeletedHandler_;
    TimelineClipSelectedHandler timelineClipSelectedHandler_;
    TimelineFocusRequestedHandler timelineFocusRequestedHandler_;
    AudioTrackVisibilityHandler audioTrackVisibilityHandler_;
    QToolButton *audioTrackVisibilityButton_ = nullptr;
    std::vector<TimelineClip> timelineClips_;
    int timelineDurationFrames_ = 600;
    TimelineClipHitRegion dragRegion_ = TimelineClipHitRegion::None;
    int dragFrameOffset_ = 0;
    int dragClipId_ = 0;
    int selectedClipId_ = 0;
    TimelineTrimContext dragTrimContext_;
    TimelineClipState dragOriginalState_;
    TimelineClipState dragPreviewState_;
    bool isMediaDropPreviewVisible_ = false;
    int mediaDropAssetId_ = 0;
    int mediaDropStartFrame_ = 0;
    TimelineAssetPresentation mediaDropPresentation_;
};
