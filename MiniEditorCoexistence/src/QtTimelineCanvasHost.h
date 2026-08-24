#pragma once

#include "ProjectState.h"

#include <afxwin.h>

#include <functional>
#include <memory>

class QtTimelineCanvas;

class QtTimelineCanvasHost final
{
public:
    using SeekHandler = std::function<void(int frame)>;
    using TimelineClipEditedHandler = std::function<void(const TimelineClipState &state)>;

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

private:
    std::unique_ptr<QtTimelineCanvas> canvas_;
};
