#pragma once

#include "ProjectState.h"
#include "TimelineClipEdit.h"
#include "TimelineAssetPresentation.h"
#include "TimelineModel.h"
#include "TimelinePresentationStateResolver.h"

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
    using ClipThumbnailResolver = std::function<QImage(const TimelineClip &clip,
                                                        int sourceFrame)>;
    using TimelineClipDeletedHandler = std::function<void(int clipId)>;
    using TimelineClipSelectedHandler = std::function<void(int clipId)>;
    using TimelineFocusRequestedHandler = std::function<void()>;
    using AudioTrackVisibilityHandler = std::function<void(bool isVisible)>;

    QtTimelineCanvasHost();
    ~QtTimelineCanvasHost();

    bool create(void *mfcParentWindowHandle);
    void resize(const CRect &bounds);
    void setPresentationState(const TimelinePresentationState &state);
    void setSeekHandler(SeekHandler handler);
    void setTimelineClipEditedHandler(TimelineClipEditedHandler handler);
    void setMediaAssetDroppedHandler(MediaAssetDroppedHandler handler);
    void setAssetPresentationResolver(AssetPresentationResolver resolver);
    void setClipThumbnailResolver(ClipThumbnailResolver resolver);
    void setTimelineClipDeletedHandler(TimelineClipDeletedHandler handler);
    void setTimelineClipSelectedHandler(TimelineClipSelectedHandler handler);
    void setTimelineFocusRequestedHandler(TimelineFocusRequestedHandler handler);
    void setAudioTrackVisibilityHandler(AudioTrackVisibilityHandler handler);

private:
    std::unique_ptr<QScrollArea> scrollArea_;
    QtTimelineCanvas *canvas_ = nullptr;
};
