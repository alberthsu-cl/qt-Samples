#pragma once

#include "EditorChange.h"
#include "EditorHistory.h"
#include "EditorProject.h"
#include "ProjectState.h"
#include "TimelineClipEdit.h"
#include "TimelineModel.h"

#include <cstddef>
#include <functional>
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
    const TimelineViewState &timelineViewState() const;
    EditorProject projectSnapshot() const;
    const TimelineModel &timelineModel() const;
    int selectedTimelineClipId() const;
    bool isTimelineFocused() const;
    int addTimelineClip(int mediaAssetId, TimelineTrackType trackType, int startFrame,
                        int durationFrames = TimelineClipState{}.durationFrames);
    bool moveTimelineClip(int clipId, const TimelineClipState &state,
                          TimelineClipEditKind editKind = TimelineClipEditKind::Move);
    int splitTimelineClip(int clipId, int splitFrame, MediaKind mediaKind);
    bool removeTimelineClip(int clipId);
    void selectTimelineClip(int clipId);
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
    void updateTimelineViewState(const TimelineViewState &state);
    void fitTimeline();

    // Restoring settings happens before views are constructed, so this does
    // not notify. The caller performs one initial view refresh afterwards.
    void restoreWorkspaceState(int selectedAssetIndex,
                               const TimelineViewState &timelineViewState);
    ObserverId addObserver(StateChangedHandler handler);
    void removeObserver(ObserverId observerId);

private:
    EditorSelectionState selectionState() const;
    EditorCommandContext commandContext();
    void recordTimelineCommand(std::vector<TimelineClip> before,
                               EditorSelectionState selectionBefore);
    void notifyStateChanged(EditorChange changes);

    std::vector<ClipSettings> clipSettings_;
    std::vector<TimelineClipState> timelineClipStates_;
    TimelineModel timelineModel_;
    int selectedAssetIndex_ = 0;
    int selectedTimelineClipId_ = 0;
    bool isTimelineFocused_ = false;
    PlaybackState playbackState_;
    TimelineViewState timelineViewState_;
    bool projectDirty_ = false;
    EditorHistory history_;
    struct Observer {
        ObserverId id;
        StateChangedHandler handler;
    };

    std::vector<Observer> observers_;
    ObserverId nextObserverId_ = 1;
};
