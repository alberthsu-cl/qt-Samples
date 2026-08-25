#pragma once

#include "EditorProject.h"
#include "ProjectState.h"
#include "TimelineModel.h"

#include <cstddef>
#include <functional>
#include <vector>

// A bitmask describing exactly which part of editor state changed. Keeping
// this framework-neutral lets MFC and Qt decide which views need refreshing.
enum class EditorChange : unsigned int {
    None = 0,
    Selection = 1 << 0,
    ClipSettings = 1 << 1,
    Playback = 1 << 2,
    TimelineView = 1 << 3,
    TimelineClip = 1 << 4,
    All = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4)
};

constexpr EditorChange operator|(EditorChange left, EditorChange right)
{
    return static_cast<EditorChange>(static_cast<unsigned int>(left)
        | static_cast<unsigned int>(right));
}

constexpr bool includesChange(EditorChange changes, EditorChange requestedChange)
{
    return (static_cast<unsigned int>(changes) & static_cast<unsigned int>(requestedChange)) != 0;
}

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
    int addTimelineClip(int mediaAssetId, TimelineTrackType trackType, int startFrame,
                        int durationFrames = TimelineClipState{}.durationFrames);
    bool moveTimelineClip(int clipId, const TimelineClipState &state);
    bool removeTimelineClip(int clipId);
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
    void updateTimelineViewState(const TimelineViewState &state);
    void fitTimeline();

    // Restoring settings happens before views are constructed, so this does
    // not notify. The caller performs one initial view refresh afterwards.
    void restoreWorkspaceState(int selectedAssetIndex,
                               const TimelineViewState &timelineViewState);
    ObserverId addObserver(StateChangedHandler handler);
    void removeObserver(ObserverId observerId);

private:
    enum class HistoryEntryType {
        ClipSettings,
        TimelineClip,
        TimelineModelMove,
        TimelineModelAdd,
        TimelineModelRemove
    };

    struct HistoryEntry {
        HistoryEntryType type;
        int assetIndex;
        ClipSettings clipSettingsBefore;
        ClipSettings clipSettingsAfter;
        TimelineClipState timelineClipBefore;
        TimelineClipState timelineClipAfter;
        int timelineClipId = 0;
        TimelineClip timelineClip;
    };

    void notifyStateChanged(EditorChange changes);

    std::vector<ClipSettings> clipSettings_;
    std::vector<TimelineClipState> timelineClipStates_;
    TimelineModel timelineModel_;
    int selectedAssetIndex_ = 0;
    PlaybackState playbackState_;
    TimelineViewState timelineViewState_;
    bool projectDirty_ = false;
    std::vector<HistoryEntry> undoHistory_;
    std::vector<HistoryEntry> redoHistory_;
    struct Observer {
        ObserverId id;
        StateChangedHandler handler;
    };

    std::vector<Observer> observers_;
    ObserverId nextObserverId_ = 1;
};
