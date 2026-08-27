#pragma once

#include "ProjectState.h"
#include "TimelineClipEdit.h"
#include "TimelineAssetPresentation.h"
#include "TimelineModel.h"

#include <afxwin.h>

#include <QColor>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

class QtTimelineCanvas;
class QScrollArea;

class QtTimelineCanvasHost final
{
public:
    using SeekHandler = std::function<void(int frame)>;
    using TimelineClipEditedHandler = std::function<void(int clipId,
                                                         const TimelineClipState &state,
                                                         TimelineClipEditKind editKind)>;
    using MediaAssetDroppedHandler = std::function<void(int mediaAssetId, int frame)>;
    using AssetPresentationResolver =
        std::function<std::optional<TimelineAssetPresentation>(int mediaAssetId)>;
    using TimelineClipDeletedHandler = std::function<void(int clipId)>;
    using TimelineClipSelectedHandler = std::function<void(int clipId)>;
    using TimelineFocusRequestedHandler = std::function<void()>;
    using AudioTrackVisibilityHandler = std::function<void(bool isVisible)>;

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
    void setSelectedClipId(int clipId);
    void setTimelineDuration(int durationFrames);
    void setMediaAssetDroppedHandler(MediaAssetDroppedHandler handler);
    void setAssetPresentationResolver(AssetPresentationResolver resolver);
    void setTimelineClipDeletedHandler(TimelineClipDeletedHandler handler);
    void setTimelineClipSelectedHandler(TimelineClipSelectedHandler handler);
    void setTimelineFocusRequestedHandler(TimelineFocusRequestedHandler handler);
    void setAudioTrackVisibilityHandler(AudioTrackVisibilityHandler handler);

private:
    std::unique_ptr<QScrollArea> scrollArea_;
    QtTimelineCanvas *canvas_ = nullptr;
};
