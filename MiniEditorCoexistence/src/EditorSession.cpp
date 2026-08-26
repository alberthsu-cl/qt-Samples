#include "EditorSession.h"
#include "TimelineTrackPolicy.h"

#include "DemoProject.h"

#include <algorithm>
#include <utility>

namespace {

constexpr int kFirstFrame = 0;
constexpr int kMinimumTimelineZoom = 50;
constexpr int kMaximumTimelineZoom = 200;
constexpr int kMaximumTimelineFrame = 600;
constexpr int kMinimumOpacityPercent = 0;
constexpr int kMaximumOpacityPercent = 100;
constexpr int kMinimumScalePercent = 25;
constexpr int kMaximumScalePercent = 200;

bool hasSameClipSettings(const ClipSettings &left, const ClipSettings &right)
{
    return left.opacityPercent == right.opacityPercent
        && left.scalePercent == right.scalePercent
        && left.position == right.position;
}

ClipSettings clampedClipSettings(ClipSettings settings)
{
    settings.opacityPercent = std::clamp(settings.opacityPercent,
                                         kMinimumOpacityPercent, kMaximumOpacityPercent);
    settings.scalePercent = std::clamp(settings.scalePercent,
                                       kMinimumScalePercent, kMaximumScalePercent);
    return settings;
}

bool hasSameTimelineClipState(const TimelineClipState &left, const TimelineClipState &right)
{
    return left.startFrame == right.startFrame
        && left.durationFrames == right.durationFrames
        && left.sourceInFrame == right.sourceInFrame;
}

TimelineClipState clampedTimelineClipState(TimelineClipState state)
{
    state.durationFrames = std::clamp(state.durationFrames, 1, kMaximumTimelineFrame);
    state.startFrame = std::clamp(state.startFrame, 0,
                                  kMaximumTimelineFrame - state.durationFrames);
    state.sourceInFrame = std::max(0, state.sourceInFrame);
    return state;
}

TimelineClipState clampedTimelineModelClipState(TimelineClipState state)
{
    // Model clips may extend the project beyond the original learning-sample
    // range. Only prevent negative values here; TimelineModel calculates the
    // resulting project duration dynamically.
    state.startFrame = std::max(0, state.startFrame);
    state.durationFrames = std::max(1, state.durationFrames);
    state.sourceInFrame = std::max(0, state.sourceInFrame);
    return state;
}

void shiftFollowingClips(std::vector<TimelineClip> &clips,
                         TimelineTrackType trackType,
                         int firstFrame,
                         int frameDelta,
                         int ignoredClipId = 0)
{
    for (TimelineClip &clip : clips) {
        if (clip.trackType == trackType && clip.id != ignoredClipId
            && clip.state.startFrame >= firstFrame) {
            clip.state.startFrame += frameDelta;
        }
    }
}

} // namespace

EditorSession::EditorSession(std::size_t assetCount)
    : clipSettings_(std::max<std::size_t>(assetCount, 1))
    , timelineClipStates_(clipSettings_.size())
{
}

int EditorSession::selectedAssetIndex() const
{
    return selectedAssetIndex_;
}

const ClipSettings &EditorSession::selectedClipSettings() const
{
    if (const TimelineClip *clip = timelineModel_.findClip(selectedTimelineClipId_))
        return clip->settings;
    return clipSettings_[selectedAssetIndex_];
}

const TimelineClipState &EditorSession::selectedTimelineClipState() const
{
    return timelineClipStates_[selectedAssetIndex_];
}

const PlaybackState &EditorSession::playbackState() const
{
    return playbackState_;
}

const TimelineViewState &EditorSession::timelineViewState() const
{
    return timelineViewState_;
}

EditorProject EditorSession::projectSnapshot() const
{
    return { {}, clipSettings_, timelineClipStates_, timelineModel_.clips() };
}

const TimelineModel &EditorSession::timelineModel() const
{
    return timelineModel_;
}

int EditorSession::selectedTimelineClipId() const
{
    return selectedTimelineClipId_;
}

bool EditorSession::isTimelineFocused() const
{
    return isTimelineFocused_;
}

int EditorSession::addTimelineClip(int mediaAssetId, TimelineTrackType trackType,
                                   int startFrame, int durationFrames)
{
    TimelineClipState state;
    state.durationFrames = std::max(1, durationFrames);
    state.startFrame = timelineViewState_.isRippleEditingEnabled
        ? TimelineTrackPolicy::rippleInsertionStart(
              timelineModel_.clips(), trackType, startFrame, 0)
        : TimelineTrackPolicy::nearestAvailableStart(
              timelineModel_.clips(), trackType, startFrame, state.durationFrames);

    if (timelineViewState_.isRippleEditingEnabled) {
        const std::vector<TimelineClip> before = timelineModel_.clips();
        std::vector<TimelineClip> shifted = before;
        shiftFollowingClips(shifted, trackType, state.startFrame,
                            state.durationFrames);
        if (!timelineModel_.replaceClips(shifted))
            return 0;

        const int clipId = timelineModel_.addClip(mediaAssetId, trackType, state);
        if (clipId == 0) {
            timelineModel_.replaceClips(before);
            return 0;
        }

        HistoryEntry entry{};
        entry.type = HistoryEntryType::TimelineModelBatch;
        entry.assetIndex = selectedAssetIndex_;
        entry.timelineClipId = clipId;
        entry.timelineBefore = before;
        entry.timelineAfter = timelineModel_.clips();
        projectDirty_ = true;
        undoHistory_.push_back(std::move(entry));
        redoHistory_.clear();
        notifyStateChanged(EditorChange::TimelineClip);
        return clipId;
    }

    const int clipId = timelineModel_.addClip(mediaAssetId, trackType, state);
    const TimelineClip *addedClip = timelineModel_.findClip(clipId);
    if (addedClip == nullptr)
        return 0;

    projectDirty_ = true;
    undoHistory_.push_back({ HistoryEntryType::TimelineModelAdd, selectedAssetIndex_,
                             {}, {}, {}, {}, clipId, *addedClip });
    redoHistory_.clear();
    notifyStateChanged(EditorChange::TimelineClip);
    return clipId;
}

bool EditorSession::moveTimelineClip(int clipId, const TimelineClipState &state,
                                     TimelineClipEditKind editKind)
{
    const TimelineClip *clip = timelineModel_.findClip(clipId);
    const TimelineClipState updatedState = clampedTimelineModelClipState(state);
    if (clip == nullptr || hasSameTimelineClipState(clip->state, updatedState))
        return false;

    const TimelineClipState previousState = clip->state;
    if (timelineViewState_.isRippleEditingEnabled
        && editKind == TimelineClipEditKind::Move) {
        const std::vector<TimelineClip> before = timelineModel_.clips();
        std::vector<TimelineClip> after;
        after.reserve(before.size());
        const int previousEnd = previousState.startFrame
            + previousState.durationFrames;
        for (const TimelineClip &candidate : before) {
            if (candidate.id == clipId)
                continue;
            TimelineClip remaining = candidate;
            if (remaining.trackType == clip->trackType
                && remaining.state.startFrame >= previousEnd) {
                remaining.state.startFrame -= previousState.durationFrames;
            }
            after.push_back(remaining);
        }

        shiftFollowingClips(after, clip->trackType, updatedState.startFrame,
                            updatedState.durationFrames);
        TimelineClip movedClip = *clip;
        movedClip.state = updatedState;
        after.push_back(movedClip);
        if (!timelineModel_.replaceClips(after))
            return false;

        HistoryEntry entry{};
        entry.type = HistoryEntryType::TimelineModelBatch;
        entry.assetIndex = selectedAssetIndex_;
        entry.timelineClipId = clipId;
        entry.timelineBefore = before;
        entry.timelineAfter = timelineModel_.clips();
        projectDirty_ = true;
        undoHistory_.push_back(std::move(entry));
        redoHistory_.clear();
        notifyStateChanged(EditorChange::TimelineClip);
        return true;
    }

    if (timelineViewState_.isRippleEditingEnabled
        && editKind != TimelineClipEditKind::Move) {
        const std::vector<TimelineClip> before = timelineModel_.clips();
        std::vector<TimelineClip> after = before;
        const auto edited = std::find_if(after.begin(), after.end(),
            [clipId](const TimelineClip &candidate) {
                return candidate.id == clipId;
            });
        if (edited == after.end())
            return false;

        TimelineClipState rippleState = updatedState;
        if (editKind == TimelineClipEditKind::TrimStart)
            rippleState.startFrame = previousState.startFrame;
        const int previousEnd = previousState.startFrame
            + previousState.durationFrames;
        const int updatedEnd = rippleState.startFrame
            + rippleState.durationFrames;
        edited->state = rippleState;
        shiftFollowingClips(after, edited->trackType, previousEnd,
                            updatedEnd - previousEnd, clipId);
        if (!timelineModel_.replaceClips(after))
            return false;

        HistoryEntry entry{};
        entry.type = HistoryEntryType::TimelineModelBatch;
        entry.assetIndex = selectedAssetIndex_;
        entry.timelineClipId = clipId;
        entry.timelineBefore = before;
        entry.timelineAfter = timelineModel_.clips();
        projectDirty_ = true;
        undoHistory_.push_back(std::move(entry));
        redoHistory_.clear();
        notifyStateChanged(EditorChange::TimelineClip);
        return true;
    }

    if (!timelineModel_.moveClip(clipId, updatedState))
        return false;

    projectDirty_ = true;
    undoHistory_.push_back({ HistoryEntryType::TimelineModelMove, selectedAssetIndex_,
                             {}, {}, previousState, updatedState, clipId });
    redoHistory_.clear();
    notifyStateChanged(EditorChange::TimelineClip);
    return true;
}

bool EditorSession::removeTimelineClip(int clipId)
{
    const TimelineClip *clip = timelineModel_.findClip(clipId);
    if (clip == nullptr)
        return false;

    const TimelineClip removedClip = *clip;
    if (timelineViewState_.isRippleEditingEnabled) {
        const std::vector<TimelineClip> before = timelineModel_.clips();
        std::vector<TimelineClip> after;
        after.reserve(before.size() - 1);
        for (const TimelineClip &candidate : before) {
            if (candidate.id != clipId)
                after.push_back(candidate);
        }
        shiftFollowingClips(after, removedClip.trackType,
                            removedClip.state.startFrame
                                + removedClip.state.durationFrames,
                            -removedClip.state.durationFrames);
        if (!timelineModel_.replaceClips(after))
            return false;

        HistoryEntry entry{};
        entry.type = HistoryEntryType::TimelineModelBatch;
        entry.assetIndex = selectedAssetIndex_;
        entry.timelineClipId = clipId;
        entry.timelineBefore = before;
        entry.timelineAfter = timelineModel_.clips();
        projectDirty_ = true;
        const bool removedSelection = selectedTimelineClipId_ == clipId;
        if (removedSelection)
            selectedTimelineClipId_ = 0;
        undoHistory_.push_back(std::move(entry));
        redoHistory_.clear();
        notifyStateChanged(EditorChange::TimelineClip
            | (removedSelection ? EditorChange::Selection : EditorChange::None));
        return true;
    }

    if (!timelineModel_.removeClip(clipId))
        return false;

    projectDirty_ = true;
    const bool removedSelection = selectedTimelineClipId_ == clipId;
    if (removedSelection)
        selectedTimelineClipId_ = 0;
    undoHistory_.push_back({ HistoryEntryType::TimelineModelRemove, selectedAssetIndex_,
                             {}, {}, {}, {}, clipId, removedClip });
    redoHistory_.clear();
    notifyStateChanged(EditorChange::TimelineClip
        | (removedSelection ? EditorChange::Selection : EditorChange::None));
    return true;
}

int EditorSession::splitTimelineClip(int clipId, int splitFrame,
                                     MediaKind mediaKind)
{
    const TimelineClip *clip = timelineModel_.findClip(clipId);
    if (clip == nullptr)
        return 0;

    const int clipStart = clip->state.startFrame;
    const int clipEnd = clipStart + clip->state.durationFrames;
    if (splitFrame <= clipStart || splitFrame >= clipEnd)
        return 0;

    const std::vector<TimelineClip> before = timelineModel_.clips();
    const int selectionBefore = selectedTimelineClipId_;
    const bool timelineFocusedBefore = isTimelineFocused_;
    const int rightClipId = timelineModel_.splitClip(clipId, splitFrame, mediaKind);
    if (rightClipId == 0)
        return 0;

    selectedTimelineClipId_ = rightClipId;
    isTimelineFocused_ = true;
    HistoryEntry entry{};
    entry.type = HistoryEntryType::TimelineModelBatch;
    entry.assetIndex = selectedAssetIndex_;
    entry.timelineClipId = rightClipId;
    entry.timelineBefore = before;
    entry.timelineAfter = timelineModel_.clips();
    entry.selectedTimelineClipBefore = selectionBefore;
    entry.selectedTimelineClipAfter = rightClipId;
    entry.timelineFocusedBefore = timelineFocusedBefore;
    entry.timelineFocusedAfter = true;
    projectDirty_ = true;
    undoHistory_.push_back(std::move(entry));
    redoHistory_.clear();
    notifyStateChanged(EditorChange::Selection | EditorChange::TimelineClip);
    return rightClipId;
}

void EditorSession::selectTimelineClip(int clipId)
{
    if (timelineModel_.findClip(clipId) == nullptr)
        return;
    selectedTimelineClipId_ = clipId;
    isTimelineFocused_ = true;
    notifyStateChanged(EditorChange::Selection);
}

void EditorSession::focusTimeline()
{
    selectedTimelineClipId_ = 0;
    isTimelineFocused_ = true;
    notifyStateChanged(EditorChange::Selection);
}

void EditorSession::addMediaAsset()
{
    clipSettings_.push_back({});
    timelineClipStates_.push_back({});
    selectedAssetIndex_ = static_cast<int>(clipSettings_.size()) - 1;
    projectDirty_ = true;
    undoHistory_.clear();
    redoHistory_.clear();
    notifyStateChanged(EditorChange::All);
}

bool EditorSession::removeMediaAsset(int assetIndex)
{
    if (assetIndex < 0 || assetIndex >= static_cast<int>(clipSettings_.size()))
        return false;

    clipSettings_.erase(clipSettings_.begin() + assetIndex);
    timelineClipStates_.erase(timelineClipStates_.begin() + assetIndex);
    selectedAssetIndex_ = std::clamp(selectedAssetIndex_, 0,
                                     static_cast<int>(clipSettings_.size()) - 1);
    projectDirty_ = true;
    undoHistory_.clear();
    redoHistory_.clear();
    notifyStateChanged(EditorChange::All);
    return true;
}

bool EditorSession::isProjectDirty() const
{
    return projectDirty_;
}

void EditorSession::selectAsset(int assetIndex)
{
    selectedAssetIndex_ = std::clamp(assetIndex, 0,
                                     static_cast<int>(clipSettings_.size()) - 1);
    selectedTimelineClipId_ = 0;
    isTimelineFocused_ = false;
    notifyStateChanged(EditorChange::Selection);
}

void EditorSession::updateSelectedClipSettings(const ClipSettings &settings)
{
    if (const TimelineClip *clip = timelineModel_.findClip(selectedTimelineClipId_)) {
        const ClipSettings previousSettings = clip->settings;
        const ClipSettings updatedSettings = clampedClipSettings(settings);
        if (hasSameClipSettings(previousSettings, updatedSettings))
            return;
        timelineModel_.updateClipSettings(selectedTimelineClipId_, updatedSettings);
        projectDirty_ = true;
        undoHistory_.push_back({ HistoryEntryType::TimelineModelSettings,
                                 selectedAssetIndex_, previousSettings, updatedSettings,
                                 {}, {}, selectedTimelineClipId_ });
        redoHistory_.clear();
        notifyStateChanged(EditorChange::ClipSettings | EditorChange::TimelineClip);
        return;
    }

    const ClipSettings previousSettings = clipSettings_[selectedAssetIndex_];
    const ClipSettings updatedSettings = clampedClipSettings(settings);
    if (hasSameClipSettings(previousSettings, updatedSettings))
        return;

    clipSettings_[selectedAssetIndex_] = updatedSettings;
    projectDirty_ = true;
    undoHistory_.push_back({ HistoryEntryType::ClipSettings, selectedAssetIndex_,
                             previousSettings, updatedSettings, {}, {} });
    redoHistory_.clear();
    notifyStateChanged(EditorChange::ClipSettings);
}

void EditorSession::updateSelectedTimelineClipState(const TimelineClipState &state)
{
    const TimelineClipState previousState = timelineClipStates_[selectedAssetIndex_];
    const TimelineClipState updatedState = clampedTimelineClipState(state);
    if (hasSameTimelineClipState(previousState, updatedState))
        return;

    timelineClipStates_[selectedAssetIndex_] = updatedState;
    projectDirty_ = true;
    undoHistory_.push_back({ HistoryEntryType::TimelineClip, selectedAssetIndex_,
                             {}, {}, previousState, updatedState });
    redoHistory_.clear();
    notifyStateChanged(EditorChange::TimelineClip);
}

void EditorSession::replaceProject(const EditorProject &project)
{
    if (project.clipSettings.empty()
        || project.clipSettings.size() != project.timelineClips.size()) {
        return;
    }

    TimelineModel restoredTimeline;
    if (!restoredTimeline.replaceClips(project.timelineItems))
        return;

    clipSettings_.resize(project.clipSettings.size());
    timelineClipStates_.resize(project.timelineClips.size());
    for (std::size_t index = 0; index < project.clipSettings.size(); ++index) {
        clipSettings_[index] = clampedClipSettings(project.clipSettings[index]);
        timelineClipStates_[index] = clampedTimelineClipState(project.timelineClips[index]);
    }
    timelineModel_ = std::move(restoredTimeline);
    selectedAssetIndex_ = std::clamp(selectedAssetIndex_, 0,
                                     static_cast<int>(clipSettings_.size()) - 1);
    selectedTimelineClipId_ = 0;
    isTimelineFocused_ = false;
    projectDirty_ = false;
    undoHistory_.clear();
    redoHistory_.clear();
    playbackState_.isPlaying = false;
    playbackState_.isPaused = false;
    playbackState_.currentFrame = kFirstFrame;
    notifyStateChanged(EditorChange::All);
}

void EditorSession::markProjectSaved()
{
    projectDirty_ = false;
}

bool EditorSession::canUndo() const
{
    return !undoHistory_.empty();
}

bool EditorSession::canRedo() const
{
    return !redoHistory_.empty();
}

bool EditorSession::undo()
{
    if (!canUndo())
        return false;

    const HistoryEntry entry = undoHistory_.back();
    undoHistory_.pop_back();
    EditorChange changes = EditorChange::Selection;
    if (entry.type == HistoryEntryType::ClipSettings) {
        clipSettings_[entry.assetIndex] = entry.clipSettingsBefore;
        changes = changes | EditorChange::ClipSettings;
    } else if (entry.type == HistoryEntryType::TimelineClip) {
        timelineClipStates_[entry.assetIndex] = entry.timelineClipBefore;
        changes = changes | EditorChange::TimelineClip;
    } else if (entry.type == HistoryEntryType::TimelineModelMove) {
        timelineModel_.moveClip(entry.timelineClipId, entry.timelineClipBefore);
        changes = changes | EditorChange::TimelineClip;
    } else if (entry.type == HistoryEntryType::TimelineModelSettings) {
        timelineModel_.updateClipSettings(entry.timelineClipId, entry.clipSettingsBefore);
        selectedTimelineClipId_ = entry.timelineClipId;
        isTimelineFocused_ = true;
        changes = changes | EditorChange::ClipSettings | EditorChange::TimelineClip;
    } else if (entry.type == HistoryEntryType::TimelineModelAdd) {
        timelineModel_.removeClip(entry.timelineClipId);
        changes = changes | EditorChange::TimelineClip;
    } else if (entry.type == HistoryEntryType::TimelineModelBatch) {
        timelineModel_.replaceClips(entry.timelineBefore);
        if (entry.selectedTimelineClipBefore >= 0)
            selectedTimelineClipId_ = entry.selectedTimelineClipBefore;
        if (entry.selectedTimelineClipBefore >= 0)
            isTimelineFocused_ = entry.timelineFocusedBefore;
        changes = changes | EditorChange::TimelineClip;
    } else {
        timelineModel_.restoreClip(entry.timelineClip);
        changes = changes | EditorChange::TimelineClip;
    }
    selectedAssetIndex_ = entry.assetIndex;
    projectDirty_ = true;
    redoHistory_.push_back(entry);
    notifyStateChanged(changes);
    return true;
}

bool EditorSession::redo()
{
    if (!canRedo())
        return false;

    const HistoryEntry entry = redoHistory_.back();
    redoHistory_.pop_back();
    EditorChange changes = EditorChange::Selection;
    if (entry.type == HistoryEntryType::ClipSettings) {
        clipSettings_[entry.assetIndex] = entry.clipSettingsAfter;
        changes = changes | EditorChange::ClipSettings;
    } else if (entry.type == HistoryEntryType::TimelineClip) {
        timelineClipStates_[entry.assetIndex] = entry.timelineClipAfter;
        changes = changes | EditorChange::TimelineClip;
    } else if (entry.type == HistoryEntryType::TimelineModelMove) {
        timelineModel_.moveClip(entry.timelineClipId, entry.timelineClipAfter);
        changes = changes | EditorChange::TimelineClip;
    } else if (entry.type == HistoryEntryType::TimelineModelSettings) {
        timelineModel_.updateClipSettings(entry.timelineClipId, entry.clipSettingsAfter);
        selectedTimelineClipId_ = entry.timelineClipId;
        isTimelineFocused_ = true;
        changes = changes | EditorChange::ClipSettings | EditorChange::TimelineClip;
    } else if (entry.type == HistoryEntryType::TimelineModelAdd) {
        timelineModel_.restoreClip(entry.timelineClip);
        changes = changes | EditorChange::TimelineClip;
    } else if (entry.type == HistoryEntryType::TimelineModelBatch) {
        timelineModel_.replaceClips(entry.timelineAfter);
        if (entry.selectedTimelineClipAfter >= 0)
            selectedTimelineClipId_ = entry.selectedTimelineClipAfter;
        if (entry.selectedTimelineClipAfter >= 0)
            isTimelineFocused_ = entry.timelineFocusedAfter;
        changes = changes | EditorChange::TimelineClip;
    } else {
        timelineModel_.removeClip(entry.timelineClipId);
        changes = changes | EditorChange::TimelineClip;
    }
    selectedAssetIndex_ = entry.assetIndex;
    projectDirty_ = true;
    undoHistory_.push_back(entry);
    notifyStateChanged(changes);
    return true;
}

void EditorSession::handlePlaybackCommand(PlaybackCommand command)
{
    switch (command) {
    case PlaybackCommand::TogglePlayPause:
        if (playbackState_.isPlaying) {
            playbackState_.isPlaying = false;
            playbackState_.isPaused = true;
        } else {
            playbackState_.isPlaying = true;
            playbackState_.isPaused = false;
        }
        break;
    case PlaybackCommand::Stop:
        playbackState_.isPlaying = false;
        playbackState_.isPaused = false;
        playbackState_.currentFrame = kFirstFrame;
        break;
    case PlaybackCommand::StepBackward:
        playbackState_.isPlaying = false;
        playbackState_.isPaused = true;
        playbackState_.currentFrame = std::max(kFirstFrame, playbackState_.currentFrame - 1);
        break;
    case PlaybackCommand::StepForward:
        playbackState_.isPlaying = false;
        playbackState_.isPaused = true;
        playbackState_.currentFrame = std::min(playbackState_.durationFrames - 1,
                                               playbackState_.currentFrame + 1);
        break;
    }

    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::advancePlaybackFrame()
{
    if (!playbackState_.isPlaying)
        return;

    ++playbackState_.currentFrame;
    if (playbackState_.currentFrame >= playbackState_.durationFrames) {
        playbackState_.currentFrame = std::max(0, playbackState_.durationFrames - 1);
        playbackState_.isPlaying = false;
        playbackState_.isPaused = true;
    }
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::seekTimeline(int frame)
{
    playbackState_.currentFrame = std::clamp(frame, kFirstFrame,
                                             std::max(0, playbackState_.durationFrames - 1));
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::setPlaybackDuration(int durationFrames, bool resetToBeginning)
{
    playbackState_.durationFrames = std::max(1, durationFrames);
    playbackState_.isPlaying = false;
    playbackState_.isPaused = false;
    playbackState_.currentFrame = resetToBeginning
        ? 0 : std::min(playbackState_.currentFrame, playbackState_.durationFrames - 1);
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::updateTimelineViewState(const TimelineViewState &state)
{
    timelineViewState_ = state;
    timelineViewState_.zoomPercent = std::clamp(timelineViewState_.zoomPercent,
                                                 kMinimumTimelineZoom,
                                                 kMaximumTimelineZoom);
    notifyStateChanged(EditorChange::TimelineView);
}

void EditorSession::fitTimeline()
{
    timelineViewState_.zoomPercent = 100;
    notifyStateChanged(EditorChange::TimelineView);
}

void EditorSession::restoreWorkspaceState(int selectedAssetIndex,
                                          const TimelineViewState &timelineViewState)
{
    selectedAssetIndex_ = std::clamp(selectedAssetIndex, 0,
                                     static_cast<int>(clipSettings_.size()) - 1);
    timelineViewState_ = timelineViewState;
    timelineViewState_.zoomPercent = std::clamp(timelineViewState_.zoomPercent,
                                                 kMinimumTimelineZoom,
                                                 kMaximumTimelineZoom);
}

EditorSession::ObserverId EditorSession::addObserver(StateChangedHandler handler)
{
    const ObserverId observerId = nextObserverId_++;
    observers_.push_back({ observerId, std::move(handler) });
    return observerId;
}

void EditorSession::removeObserver(ObserverId observerId)
{
    observers_.erase(std::remove_if(observers_.begin(), observers_.end(),
        [observerId](const Observer &observer) { return observer.id == observerId; }),
        observers_.end());
}

void EditorSession::notifyStateChanged(EditorChange changes)
{
    // Copy first: an observer may safely subscribe or unsubscribe while a
    // notification is being delivered without invalidating this iteration.
    const std::vector<Observer> observers = observers_;
    for (const Observer &observer : observers) {
        if (observer.handler)
            observer.handler(changes);
    }
}
