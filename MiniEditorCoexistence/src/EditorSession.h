#pragma once

#include "EditorChange.h"
#include "EditorHistory.h"
#include "EditorProject.h"
#include "ProjectState.h"
#include "TimelineClipEdit.h"
#include "TimelineModel.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

// Framework-neutral editor state and commands. It knows no MFC window and no
// Qt object. UI frameworks may request changes, but this session owns the
// resulting selection, clip, playback, and timeline state.
class EditorSession final
{
public:
    using StateChangedHandler = std::function<void(EditorChange changes)>;
    using ObserverId = std::size_t;

    explicit EditorSession(std::size_t assetCount);

    int selectedAssetIndex() const;
    const ClipSettings &selectedClipSettings() const;
    const TimelineClipState &selectedTimelineClipState() const;
    const PlaybackState &playbackState() const;
    const PlaybackState &sourcePlaybackState() const;
    const PlaybackState &timelinePlaybackState() const;
    const TimelineViewState &timelineViewState() const;
    EditorProject projectSnapshot() const;
    const TimelineModel &timelineModel() const;
    int selectedTimelineClipId() const;
    bool isTimelineFocused() const;
    int addTimelineClip(int mediaAssetId, TimelineTrackType trackType, int startFrame,
                        int durationFrames = TimelineClipState{}.durationFrames);
    int insertTimelineClip(int mediaAssetId, TimelineTrackType trackType,
                           int startFrame, int durationFrames,
                           int sourceAssetIndex);
    bool moveTimelineClip(int clipId, const TimelineClipState &state,
                          TimelineClipEditKind editKind = TimelineClipEditKind::Move);
    int splitTimelineClip(int clipId, int splitFrame, MediaKind mediaKind);
    bool removeTimelineClip(int clipId);
    bool copySelectedTimelineClip();
    bool cutSelectedTimelineClip();
    int pasteTimelineClip(int startFrame);
    int duplicateSelectedTimelineClip();
    bool hasTimelineClipboard() const;
    int timelineClipboardMediaAssetId() const;
    void selectTimelineClip(int clipId);
    void selectTimelineClip(int clipId, int assetIndex);
    void focusTimeline();
    void addMediaAsset();
    bool removeMediaAsset(int assetIndex);
    bool isProjectDirty() const;

    void selectAsset(int assetIndex);
    void updateSelectedClipSettings(const ClipSettings &settings);
    void updateSelectedTimelineClipState(const TimelineClipState &state);
    void replaceProject(const EditorProject &project);
    void markProjectSaved();
    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();
    void handlePlaybackCommand(PlaybackCommand command);
    void advancePlaybackFrame();
    void seekTimeline(int frame);
    void setPlaybackDuration(int durationFrames, bool resetToBeginning);
    void updatePlaybackFromBackend(int currentFrame, int durationFrames,
                                   bool isPlaying, bool isPaused);
    void updateTimelineViewState(const TimelineViewState &state);
    void fitTimeline();

    // Restoring settings happens before views are constructed, so this does
    // not notify. The caller performs one initial view refresh afterwards.
    void restoreWorkspaceState(const TimelineViewState &timelineViewState);
    ObserverId addObserver(StateChangedHandler handler);
    void removeObserver(ObserverId observerId);

private:
    EditorSelectionState selectionState() const;
    TimelineInteractionState timelineInteractionState() const;
    EditorCommandContext commandContext();
    void recordTimelineCommand(std::vector<TimelineClip> before,
                               TimelineInteractionState interactionBefore);
    int addTimelineClipInternal(int mediaAssetId, TimelineTrackType trackType,
                                int startFrame, int durationFrames,
                                std::optional<int> sourceAssetIndex);
    PlaybackState &activePlaybackState();
    const PlaybackState &activePlaybackState() const;
    int insertTimelineClipCopy(const TimelineClip &sourceClip,
                               int sourceAssetIndex, int desiredStartFrame);
    void notifyStateChanged(EditorChange changes);

    std::vector<ClipSettings> clipSettings_;
    std::vector<TimelineClipState> timelineClipStates_;
    TimelineModel timelineModel_;
    // Selection is transient UI state. A new application session starts with
    // no source asset or timeline clip focused.
    int selectedAssetIndex_ = -1;
    int selectedTimelineClipId_ = 0;
    bool isTimelineFocused_ = false;
    PlaybackState sourcePlaybackState_;
    PlaybackState timelinePlaybackState_;
    TimelineViewState timelineViewState_;
    bool projectDirty_ = false;
    EditorHistory history_;
    struct TimelineClipboard {
        TimelineClip clip;
        int sourceAssetIndex = 0;
    };
    std::optional<TimelineClipboard> timelineClipboard_;
    struct Observer {
        ObserverId id;
        StateChangedHandler handler;
    };

    std::vector<Observer> observers_;
    ObserverId nextObserverId_ = 1;
};
