#pragma once

#include "EditorChange.h"
#include "EditorHistory.h"
#include "EditorProject.h"
#include "ProjectState.h"
#include "TimelineClipEdit.h"
#include "TimelineModel.h"
#include "playback_core/ProjectRuntime.h"
#include "playback_core/TimelineTransportView.h"

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

    // M5-10. EditingSelection whenever the transport is not running: parked
    // is when editing happens, and every way of being parked -- stopped at
    // the start, paused mid-playback, stepped, or seeked -- is equally a
    // moment when the user is adjusting a clip rather than watching one.
    // Transport resumption is what clears it.
    TimelinePreviewFocus timelinePreviewFocus() const;

    // The preview panel and the speakers serve one context at a time, so
    // leaving the timeline parks its transport where it stood. ADR-002:
    // Paused means the authoritative position is frozen -- which it is, since
    // nothing advances the timeline head while a library asset is being
    // auditioned. The legacy path used to leave it flagged Playing and
    // silently resume when focus came back; returning now needs an explicit
    // Play, on both paths.
    void parkTimelineTransportForOtherContext();
    const TimelineViewState &timelineViewState() const;
    const TimelineAudioMixState &timelineAudioMixState() const;
    const mini_editor::playback_core::ProjectRuntime &projectRuntime() const;
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
    void handlePlaybackCommand(LegacyPlaybackCommand command);
    void advancePlaybackFrame();
    void seekTimeline(int frame);
    void setPlaybackDuration(int durationFrames, bool resetToBeginning);
    void updatePlaybackFromBackend(int currentFrame, int durationFrames,
                                   bool isPlaying, bool isPaused);
    void updatePlaybackRatePercent(int ratePercent);

    // ADR-002's migration strategy: with timeline preview routed through the
    // new engine, the timeline PlaybackState survives only as a painting
    // cache, and this is the one way it may be written. It stores what
    // PlaybackSession published and nothing else -- no clamping, no
    // inference, no history, no dirty flag, no sequence revision -- because
    // any of those would make the cache a second opinion about transport.
    // Nothing reads it back into the engine.
    //
    // The routed path calls this at presentation cadence, so an unchanged
    // view publishes nothing rather than repainting the timeline sixty times
    // a second.
    void adoptRoutedTimelineTransport(
        const mini_editor::playback_core::TimelineTransportView &view);

    // How many times a legacy playback mutator has changed the *timeline*
    // transport state. ADR-002 requires the routed path to call none of
    // them, and M5-07's comparison harness asserts exactly that -- a counter
    // rather than an assertion, so the harness can state the fact instead of
    // the build merely not crashing. adoptRoutedTimelineTransport() does not
    // count: it is the painting cache, not a mutator.
    std::size_t legacyTimelinePlaybackMutationCount() const;

    // M5-09, ADR-002 migration steps 5-6. The engine owns timeline transport,
    // so the legacy timeline playback mutators stop being reachable *and*
    // stop being able to act: each one returns without effect when it would
    // touch timeline transport. adoptRoutedTimelineTransport() becomes the
    // single writer of that state.
    //
    // A guard rather than deletion, because the same seven methods still
    // serve source-asset preview (Decision E) and still serve the timeline in
    // the retained compile-time fallback (Decision D). M5-07's matrix already
    // proves none of them fires on the routed path; this makes that a
    // property of the code rather than of the call graph.
    void setTimelineTransportRoutedExternally(bool routed);
    // Explicit timeline editing may replace a frozen paused preview without
    // moving the playhead. Call this before publishing the new selection so
    // every observer resolves that selection as the edit target.
    void leavePausedTimelinePlaybackForEditing();
    void updateTimelineViewState(const TimelineViewState &state);
    void updateTimelineAudioMixState(const TimelineAudioMixState &state);
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
    void synchronizeProjectRuntime();
    int addTimelineClipInternal(int mediaAssetId, TimelineTrackType trackType,
                                int startFrame, int durationFrames,
                                std::optional<int> sourceAssetIndex);
    PlaybackState &activePlaybackState();
    const PlaybackState &activePlaybackState() const;
    // Called by every legacy playback mutator; counts only when the state it
    // is about to change is the timeline's.
    // Returns false when the caller must not proceed: the mutation would
    // touch timeline transport that the engine owns. Counts the ones that do
    // proceed, which is what M5-07 asserts is zero on the routed path.
    bool beginLegacyTimelinePlaybackMutation();
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
    std::size_t legacyTimelinePlaybackMutations_ = 0;
    bool timelineTransportRoutedExternally_ = false;
    TimelineViewState timelineViewState_;
    TimelineAudioMixState timelineAudioMixState_;
    mini_editor::playback_core::ProjectRuntime projectRuntime_;
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
