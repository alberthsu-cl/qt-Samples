#pragma once

#include "ProjectState.h"
#include "TimelineModel.h"

#include <afxwin.h>

#include <functional>
#include <memory>

class QtTimelineCanvas;
class QScrollArea;

class QtTimelineCanvasHost final
{
public:
    using SeekHandler = std::function<void(int frame)>;
    using TimelineClipEditedHandler = std::function<void(int clipId,
                                                         const TimelineClipState &state)>;
    using MediaAssetDroppedHandler = std::function<void(int mediaAssetId, int frame)>;
    using TimelineClipDeletedHandler = std::function<void(int clipId)>;

    QtTimelineCanvasHost();
    ~QtTimelineCanvasHost();

    bool create(void *mfcParentWindowHandle);
    void resize(const CRect &bounds);
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
    void setTimelineClipDeletedHandler(TimelineClipDeletedHandler handler);

private:
    std::unique_ptr<QScrollArea> scrollArea_;
    QtTimelineCanvas *canvas_ = nullptr;
};
