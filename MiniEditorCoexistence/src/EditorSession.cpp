#include "EditorSession.h"

#include "DemoProject.h"

#include <algorithm>
#include <utility>

namespace {

constexpr int kFirstFrame = 0;
constexpr int kLastFrame = 299;
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
        && left.durationFrames == right.durationFrames;
}

TimelineClipState clampedTimelineClipState(TimelineClipState state)
{
    state.durationFrames = std::clamp(state.durationFrames, 1, kMaximumTimelineFrame);
    state.startFrame = std::clamp(state.startFrame, 0,
                                  kMaximumTimelineFrame - state.durationFrames);
    return state;
}

TimelineClipState clampedTimelineModelClipState(TimelineClipState state)
{
    // Model clips may extend the project beyond the original learning-sample
    // range. Only prevent negative values here; TimelineModel calculates the
    // resulting project duration dynamically.
    state.startFrame = std::max(0, state.startFrame);
    state.durationFrames = std::max(1, state.durationFrames);
    return state;
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
    return { clipSettings_, timelineClipStates_, timelineModel_.clips() };
}

const TimelineModel &EditorSession::timelineModel() const
{
    return timelineModel_;
}

int EditorSession::addTimelineClip(int mediaAssetId, TimelineTrackType trackType,
                                   int startFrame, int durationFrames)
{
    TimelineClipState state;
    state.startFrame = std::max(0, startFrame);
    state.durationFrames = std::max(1, durationFrames);
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

bool EditorSession::moveTimelineClip(int clipId, const TimelineClipState &state)
{
    const TimelineClip *clip = timelineModel_.findClip(clipId);
    const TimelineClipState updatedState = clampedTimelineModelClipState(state);
    if (clip == nullptr || hasSameTimelineClipState(clip->state, updatedState))
        return false;

    const TimelineClipState previousState = clip->state;
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
    if (!timelineModel_.removeClip(clipId))
        return false;

    projectDirty_ = true;
    undoHistory_.push_back({ HistoryEntryType::TimelineModelRemove, selectedAssetIndex_,
                             {}, {}, {}, {}, clipId, removedClip });
    redoHistory_.clear();
    notifyStateChanged(EditorChange::TimelineClip);
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
    notifyStateChanged(EditorChange::Selection);
}

void EditorSession::updateSelectedClipSettings(const ClipSettings &settings)
{
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
    // The demo media catalog is fixed, so a project must have one state entry
    // per catalog asset. Rejecting a mismatch keeps every indexed view safe.
    if (project.clipSettings.size() != clipSettings_.size()
        || project.timelineClips.size() != timelineClipStates_.size()) {
        return;
    }

    for (std::size_t index = 0; index < clipSettings_.size(); ++index) {
        clipSettings_[index] = clampedClipSettings(project.clipSettings[index]);
        timelineClipStates_[index] = clampedTimelineClipState(project.timelineClips[index]);
    }
    timelineModel_.clear();
    for (const TimelineClip &clip : project.timelineItems) {
        if (findDemoAsset(clip.mediaAssetId) == nullptr
            || !timelineModel_.restoreClip(clip)) {
            return;
        }
    }
    projectDirty_ = false;
    undoHistory_.clear();
    redoHistory_.clear();
    playbackState_.isPlaying = false;
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
    } else if (entry.type == HistoryEntryType::TimelineModelAdd) {
        timelineModel_.removeClip(entry.timelineClipId);
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
    } else if (entry.type == HistoryEntryType::TimelineModelAdd) {
        timelineModel_.restoreClip(entry.timelineClip);
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
        playbackState_.isPlaying = !playbackState_.isPlaying;
        break;
    case PlaybackCommand::Stop:
        playbackState_.isPlaying = false;
        playbackState_.currentFrame = kFirstFrame;
        break;
    case PlaybackCommand::StepBackward:
        playbackState_.isPlaying = false;
        playbackState_.currentFrame = std::max(kFirstFrame, playbackState_.currentFrame - 1);
        break;
    case PlaybackCommand::StepForward:
        playbackState_.isPlaying = false;
        playbackState_.currentFrame = std::min(kLastFrame, playbackState_.currentFrame + 1);
        break;
    }

    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::advancePlaybackFrame()
{
    if (!playbackState_.isPlaying)
        return;

    playbackState_.currentFrame = (playbackState_.currentFrame + 1) % (kLastFrame + 1);
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::seekTimeline(int frame)
{
    playbackState_.currentFrame = std::clamp(frame, kFirstFrame, kLastFrame);
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
