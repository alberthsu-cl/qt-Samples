#include "EditorSession.h"

#include "ClipFade.h"
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
        && left.position == right.position
        && left.fadeInFrames == right.fadeInFrames
        && left.fadeOutFrames == right.fadeOutFrames
        && left.effect == right.effect
        && left.effectIntensityPercent == right.effectIntensityPercent;
}

// clipDurationFrames is zero for source-library settings, which have no
// placement length yet. A timeline placement passes its own duration so a
// stored fade can never be longer than the clip it belongs to.
ClipSettings clampedClipSettings(ClipSettings settings, int clipDurationFrames = 0)
{
    settings.opacityPercent = std::clamp(settings.opacityPercent,
                                         kMinimumOpacityPercent, kMaximumOpacityPercent);
    settings.scalePercent = std::clamp(settings.scalePercent,
                                       kMinimumScalePercent, kMaximumScalePercent);
    settings.effectIntensityPercent = std::clamp(
        settings.effectIntensityPercent,
        kMinimumEffectIntensityPercent, kMaximumEffectIntensityPercent);
    return ClipFade::clampSettings(settings, clipDurationFrames);
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
    , projectRuntime_(mini_editor::playback_core::ProjectRuntime::fromLegacyFlatProject(0))
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

    static const ClipSettings noSelectionSettings;
    if (selectedAssetIndex_ < 0
        || selectedAssetIndex_ >= static_cast<int>(clipSettings_.size())) {
        return noSelectionSettings;
    }
    return clipSettings_[selectedAssetIndex_];
}

const TimelineClipState &EditorSession::selectedTimelineClipState() const
{
    static const TimelineClipState noSelectionState;
    if (selectedAssetIndex_ < 0
        || selectedAssetIndex_ >= static_cast<int>(timelineClipStates_.size())) {
        return noSelectionState;
    }
    return timelineClipStates_[selectedAssetIndex_];
}

const PlaybackState &EditorSession::playbackState() const
{
    return activePlaybackState();
}

const PlaybackState &EditorSession::sourcePlaybackState() const
{
    return sourcePlaybackState_;
}

const PlaybackState &EditorSession::timelinePlaybackState() const
{
    return timelinePlaybackState_;
}

PlaybackState &EditorSession::activePlaybackState()
{
    return isTimelineFocused_ ? timelinePlaybackState_ : sourcePlaybackState_;
}

const PlaybackState &EditorSession::activePlaybackState() const
{
    return isTimelineFocused_ ? timelinePlaybackState_ : sourcePlaybackState_;
}

void EditorSession::recordLegacyTimelinePlaybackMutation()
{
    if (isTimelineFocused_)
        ++legacyTimelinePlaybackMutations_;
}

std::size_t EditorSession::legacyTimelinePlaybackMutationCount() const
{
    return legacyTimelinePlaybackMutations_;
}

TimelinePreviewFocus EditorSession::timelinePreviewFocus() const
{
    return playbackState().isPlaying ? TimelinePreviewFocus::Transport
                                     : TimelinePreviewFocus::EditingSelection;
}

const TimelineViewState &EditorSession::timelineViewState() const
{
    return timelineViewState_;
}

const TimelineAudioMixState &EditorSession::timelineAudioMixState() const
{
    return timelineAudioMixState_;
}

const mini_editor::playback_core::ProjectRuntime &EditorSession::projectRuntime() const
{
    return projectRuntime_;
}

EditorProject EditorSession::projectSnapshot() const
{
    return { {}, clipSettings_, timelineClipStates_, timelineModel_.clips(),
             timelineAudioMixState_ };
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

EditorSelectionState EditorSession::selectionState() const
{
    return { selectedAssetIndex_, selectedTimelineClipId_, isTimelineFocused_ };
}

TimelineInteractionState EditorSession::timelineInteractionState() const
{
    return { selectionState(), sourcePlaybackState_, timelinePlaybackState_ };
}

EditorCommandContext EditorSession::commandContext()
{
    return { clipSettings_, timelineClipStates_, timelineModel_,
             selectedAssetIndex_, selectedTimelineClipId_, isTimelineFocused_,
             sourcePlaybackState_, timelinePlaybackState_ };
}

void EditorSession::recordTimelineCommand(
    std::vector<TimelineClip> before,
    TimelineInteractionState interactionBefore)
{
    synchronizeProjectRuntime();
    history_.record(std::make_unique<TimelineSnapshotCommand>(
        std::move(before), timelineModel_.clips(), interactionBefore,
        timelineInteractionState()));
    projectDirty_ = true;
}

void EditorSession::synchronizeProjectRuntime()
{
    projectRuntime_.setLegacySequenceClipCount(timelineModel_.clips().size());
    projectRuntime_.advanceActiveSequenceRevision();
}

int EditorSession::addTimelineClip(int mediaAssetId, TimelineTrackType trackType,
                                   int startFrame, int durationFrames)
{
    return addTimelineClipInternal(mediaAssetId, trackType, startFrame,
                                   durationFrames, std::nullopt);
}

int EditorSession::insertTimelineClip(int mediaAssetId,
                                      TimelineTrackType trackType,
                                      int startFrame, int durationFrames,
                                      int sourceAssetIndex)
{
    return addTimelineClipInternal(mediaAssetId, trackType, startFrame,
                                   durationFrames, sourceAssetIndex);
}

int EditorSession::addTimelineClipInternal(
    int mediaAssetId, TimelineTrackType trackType, int startFrame,
    int durationFrames, std::optional<int> sourceAssetIndex)
{
    const std::vector<TimelineClip> before = timelineModel_.clips();
    const TimelineInteractionState interactionBefore = timelineInteractionState();
    TimelineClipState state;
    state.durationFrames = std::max(1, durationFrames);
    state.startFrame = timelineViewState_.isRippleEditingEnabled
        ? TimelineTrackPolicy::rippleInsertionStart(
              timelineModel_.clips(), trackType, startFrame, 0)
        : TimelineTrackPolicy::nearestAvailableStart(
            timelineModel_.clips(), trackType, startFrame, state.durationFrames);

    const auto focusInsertedClip = [this, &state, sourceAssetIndex](int clipId) {
        if (!sourceAssetIndex)
            return EditorChange::TimelineClip;

        selectedAssetIndex_ = std::clamp(
            *sourceAssetIndex, 0, static_cast<int>(clipSettings_.size()) - 1);
        selectedTimelineClipId_ = clipId;
        isTimelineFocused_ = true;
        timelinePlaybackState_.isPlaying = false;
        timelinePlaybackState_.isPaused = false;
        timelinePlaybackState_.durationFrames = std::max(
            1, timelineModel_.contentDurationFrames());
        timelinePlaybackState_.currentFrame = std::clamp(
            state.startFrame, 0, timelinePlaybackState_.durationFrames - 1);
        return EditorChange::Selection | EditorChange::TimelineClip
            | EditorChange::Playback;
    };

    if (timelineViewState_.isRippleEditingEnabled) {
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

        const EditorChange changes = focusInsertedClip(clipId);
        recordTimelineCommand(before, interactionBefore);
        notifyStateChanged(changes);
        return clipId;
    }

    const int clipId = timelineModel_.addClip(mediaAssetId, trackType, state);
    if (timelineModel_.findClip(clipId) == nullptr)
        return 0;

    const EditorChange changes = focusInsertedClip(clipId);
    recordTimelineCommand(before, interactionBefore);
    notifyStateChanged(changes);
    return clipId;
}

bool EditorSession::moveTimelineClip(int clipId, const TimelineClipState &state,
                                     TimelineClipEditKind editKind)
{
    const TimelineClip *clip = timelineModel_.findClip(clipId);
    const TimelineClipState updatedState = clampedTimelineModelClipState(state);
    if (clip == nullptr || hasSameTimelineClipState(clip->state, updatedState))
        return false;

    const std::vector<TimelineClip> before = timelineModel_.clips();
    const TimelineInteractionState interactionBefore = timelineInteractionState();
    const TimelineClipState previousState = clip->state;
    if (timelineViewState_.isRippleEditingEnabled
        && editKind == TimelineClipEditKind::Move) {
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

        recordTimelineCommand(before, interactionBefore);
        notifyStateChanged(EditorChange::TimelineClip);
        return true;
    }

    if (timelineViewState_.isRippleEditingEnabled
        && editKind != TimelineClipEditKind::Move) {
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

        recordTimelineCommand(before, interactionBefore);
        notifyStateChanged(EditorChange::TimelineClip);
        return true;
    }

    if (!timelineModel_.moveClip(clipId, updatedState))
        return false;

    recordTimelineCommand(before, interactionBefore);
    notifyStateChanged(EditorChange::TimelineClip);
    return true;
}

bool EditorSession::removeTimelineClip(int clipId)
{
    const TimelineClip *clip = timelineModel_.findClip(clipId);
    if (clip == nullptr)
        return false;

    const std::vector<TimelineClip> before = timelineModel_.clips();
    const TimelineInteractionState interactionBefore = timelineInteractionState();
    const TimelineClip removedClip = *clip;
    if (timelineViewState_.isRippleEditingEnabled) {
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

        const bool removedSelection = selectedTimelineClipId_ == clipId;
        if (removedSelection)
            selectedTimelineClipId_ = 0;
        recordTimelineCommand(before, interactionBefore);
        notifyStateChanged(EditorChange::TimelineClip
            | (removedSelection ? EditorChange::Selection : EditorChange::None));
        return true;
    }

    if (!timelineModel_.removeClip(clipId))
        return false;

    const bool removedSelection = selectedTimelineClipId_ == clipId;
    if (removedSelection)
        selectedTimelineClipId_ = 0;
    recordTimelineCommand(before, interactionBefore);
    notifyStateChanged(EditorChange::TimelineClip
        | (removedSelection ? EditorChange::Selection : EditorChange::None));
    return true;
}

bool EditorSession::copySelectedTimelineClip()
{
    const TimelineClip *clip = timelineModel_.findClip(selectedTimelineClipId_);
    if (clip == nullptr)
        return false;

    timelineClipboard_ = TimelineClipboard{ *clip, selectedAssetIndex_ };
    return true;
}

bool EditorSession::cutSelectedTimelineClip()
{
    const int clipId = selectedTimelineClipId_;
    return copySelectedTimelineClip() && removeTimelineClip(clipId);
}

int EditorSession::pasteTimelineClip(int startFrame)
{
    if (!timelineClipboard_)
        return 0;
    return insertTimelineClipCopy(timelineClipboard_->clip,
                                  timelineClipboard_->sourceAssetIndex,
                                  startFrame);
}

int EditorSession::duplicateSelectedTimelineClip()
{
    const TimelineClip *clip = timelineModel_.findClip(selectedTimelineClipId_);
    if (clip == nullptr)
        return 0;

    const TimelineClip sourceClip = *clip;
    return insertTimelineClipCopy(
        sourceClip, selectedAssetIndex_,
        sourceClip.state.startFrame + sourceClip.state.durationFrames);
}

bool EditorSession::hasTimelineClipboard() const
{
    return timelineClipboard_.has_value();
}

int EditorSession::timelineClipboardMediaAssetId() const
{
    return timelineClipboard_ ? timelineClipboard_->clip.mediaAssetId : 0;
}

int EditorSession::insertTimelineClipCopy(
    const TimelineClip &sourceClip, int sourceAssetIndex,
    int desiredStartFrame)
{
    const std::vector<TimelineClip> before = timelineModel_.clips();
    const TimelineInteractionState interactionBefore = timelineInteractionState();
    TimelineClipState state = sourceClip.state;
    state.startFrame = timelineViewState_.isRippleEditingEnabled
        ? TimelineTrackPolicy::rippleInsertionStart(
              before, sourceClip.trackType, desiredStartFrame, 0)
        : TimelineTrackPolicy::nearestAvailableStart(
              before, sourceClip.trackType, desiredStartFrame,
              state.durationFrames);

    if (timelineViewState_.isRippleEditingEnabled) {
        std::vector<TimelineClip> shifted = before;
        shiftFollowingClips(shifted, sourceClip.trackType, state.startFrame,
                            state.durationFrames);
        if (!timelineModel_.replaceClips(shifted))
            return 0;
    }

    const int newClipId = timelineModel_.addClip(
        sourceClip.mediaAssetId, sourceClip.trackType, state);
    if (newClipId == 0) {
        if (timelineViewState_.isRippleEditingEnabled)
            timelineModel_.replaceClips(before);
        return 0;
    }
    timelineModel_.updateClipSettings(newClipId, sourceClip.settings);

    selectedAssetIndex_ = std::clamp(
        sourceAssetIndex, 0, static_cast<int>(clipSettings_.size()) - 1);
    selectedTimelineClipId_ = newClipId;
    isTimelineFocused_ = true;
    recordTimelineCommand(before, interactionBefore);
    notifyStateChanged(EditorChange::Selection | EditorChange::TimelineClip);
    return newClipId;
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
    const TimelineInteractionState interactionBefore = timelineInteractionState();
    const int rightClipId = timelineModel_.splitClip(clipId, splitFrame, mediaKind);
    if (rightClipId == 0)
        return 0;

    selectedTimelineClipId_ = rightClipId;
    isTimelineFocused_ = true;
    recordTimelineCommand(before, interactionBefore);
    notifyStateChanged(EditorChange::Selection | EditorChange::TimelineClip);
    return rightClipId;
}

void EditorSession::selectTimelineClip(int clipId)
{
    selectTimelineClip(clipId, selectedAssetIndex_);
}

void EditorSession::selectTimelineClip(int clipId, int assetIndex)
{
    if (timelineModel_.findClip(clipId) == nullptr)
        return;
    selectedAssetIndex_ = std::clamp(assetIndex, 0,
                                     static_cast<int>(clipSettings_.size()) - 1);
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
    history_.clear();
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
    history_.clear();
    timelineClipboard_.reset();
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
        const ClipSettings updatedSettings = clampedClipSettings(
            settings, clip->state.durationFrames);
        if (hasSameClipSettings(previousSettings, updatedSettings))
            return;
        timelineModel_.updateClipSettings(selectedTimelineClipId_, updatedSettings);
        projectDirty_ = true;
        // Opacity, scale, fades and effects all reach the playback snapshot,
        // so this is a playback-affecting edit and owes a strictly newer
        // revision (ADR-006 rule 4). Only the timeline-clip branch does: the
        // library-defaults branch below changes nothing the engine sees.
        synchronizeProjectRuntime();
        history_.record(std::make_unique<TimelineClipSettingsCommand>(
            selectionState(), selectedTimelineClipId_, previousSettings,
            updatedSettings));
        notifyStateChanged(EditorChange::ClipSettings | EditorChange::TimelineClip);
        return;
    }

    if (selectedAssetIndex_ < 0
        || selectedAssetIndex_ >= static_cast<int>(clipSettings_.size())) {
        return;
    }

    const ClipSettings previousSettings = clipSettings_[selectedAssetIndex_];
    const ClipSettings updatedSettings = clampedClipSettings(settings);
    if (hasSameClipSettings(previousSettings, updatedSettings))
        return;

    clipSettings_[selectedAssetIndex_] = updatedSettings;
    projectDirty_ = true;
    history_.record(std::make_unique<SourceClipSettingsCommand>(
        selectionState(), previousSettings, updatedSettings));
    notifyStateChanged(EditorChange::ClipSettings);
}

void EditorSession::updateSelectedTimelineClipState(const TimelineClipState &state)
{
    if (selectedAssetIndex_ < 0
        || selectedAssetIndex_ >= static_cast<int>(timelineClipStates_.size())) {
        return;
    }

    const TimelineClipState previousState = timelineClipStates_[selectedAssetIndex_];
    const TimelineClipState updatedState = clampedTimelineClipState(state);
    if (hasSameTimelineClipState(previousState, updatedState))
        return;

    timelineClipStates_[selectedAssetIndex_] = updatedState;
    projectDirty_ = true;
    history_.record(std::make_unique<SourceTimelineStateCommand>(
        selectionState(), previousState, updatedState));
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
    timelineAudioMixState_ = project.timelineAudioMix;
    projectRuntime_ = mini_editor::playback_core::ProjectRuntime::fromLegacyFlatProject(
        timelineModel_.clips().size());
    selectedAssetIndex_ = -1;
    selectedTimelineClipId_ = 0;
    isTimelineFocused_ = false;
    projectDirty_ = false;
    history_.clear();
    timelineClipboard_.reset();
    const int playbackRatePercent = sourcePlaybackState_.playbackRatePercent;
    sourcePlaybackState_ = {};
    timelinePlaybackState_ = {};
    sourcePlaybackState_.playbackRatePercent = playbackRatePercent;
    timelinePlaybackState_.playbackRatePercent = playbackRatePercent;
    notifyStateChanged(EditorChange::All);
}

void EditorSession::markProjectSaved()
{
    projectDirty_ = false;
}

bool EditorSession::canUndo() const
{
    return history_.canUndo();
}

bool EditorSession::canRedo() const
{
    return history_.canRedo();
}

bool EditorSession::undo()
{
    EditorCommandContext context = commandContext();
    const EditorChange changes = history_.undo(context);
    if (changes == EditorChange::None)
        return false;

    projectDirty_ = true;
    synchronizeProjectRuntime();
    notifyStateChanged(changes);
    return true;
}

bool EditorSession::redo()
{
    EditorCommandContext context = commandContext();
    const EditorChange changes = history_.redo(context);
    if (changes == EditorChange::None)
        return false;

    projectDirty_ = true;
    synchronizeProjectRuntime();
    notifyStateChanged(changes);
    return true;
}

void EditorSession::handlePlaybackCommand(LegacyPlaybackCommand command)
{
    recordLegacyTimelinePlaybackMutation();
    PlaybackState &playback = activePlaybackState();

    switch (command) {
    case LegacyPlaybackCommand::TogglePlayPause:
        if (playback.isPlaying) {
            playback.isPlaying = false;
            playback.isPaused = true;
        } else {
            playback.isPlaying = true;
            playback.isPaused = false;
        }
        break;
    case LegacyPlaybackCommand::Stop:
        playback.isPlaying = false;
        playback.isPaused = false;
        playback.currentFrame = kFirstFrame;
        break;
    case LegacyPlaybackCommand::StepBackward:
        playback.isPlaying = false;
        playback.isPaused = true;
        playback.currentFrame = std::max(kFirstFrame, playback.currentFrame - 1);
        break;
    case LegacyPlaybackCommand::StepForward:
        playback.isPlaying = false;
        playback.isPaused = true;
        playback.currentFrame = std::min(playback.durationFrames - 1,
                                         playback.currentFrame + 1);
        break;
    }

    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::advancePlaybackFrame()
{
    recordLegacyTimelinePlaybackMutation();
    PlaybackState &playback = activePlaybackState();
    if (!playback.isPlaying)
        return;

    ++playback.currentFrame;
    if (playback.currentFrame >= playback.durationFrames) {
        playback.isPlaying = false;
        if (isTimelineFocused_) {
            // A completed timeline is ready to play again immediately. This
            // matches an explicit Stop without requiring a second transport
            // click after the head reaches the total duration.
            playback.currentFrame = kFirstFrame;
            playback.isPaused = false;
        } else {
            // Source preview keeps its last decoded frame for inspection.
            playback.currentFrame = std::max(0, playback.durationFrames - 1);
            playback.isPaused = true;
        }
    }
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::seekTimeline(int frame)
{
    recordLegacyTimelinePlaybackMutation();
    PlaybackState &playback = activePlaybackState();
    playback.currentFrame = std::clamp(
        frame, kFirstFrame, std::max(0, playback.durationFrames - 1));
    // M5-10: ADR-002's command table -- "from Stopped, seek and enter
    // Paused". Stopped means the context is positioned at its defined start,
    // and after a seek it is not, so reporting Stopped was a lie this path
    // had been telling. Seeking while playing keeps playing, as before.
    if (!playback.isPlaying)
        playback.isPaused = true;
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::setPlaybackDuration(int durationFrames, bool resetToBeginning)
{
    recordLegacyTimelinePlaybackMutation();
    PlaybackState &playback = activePlaybackState();
    playback.durationFrames = std::max(1, durationFrames);
    playback.isPlaying = false;
    playback.isPaused = false;
    playback.currentFrame = resetToBeginning
        ? 0 : std::min(playback.currentFrame, playback.durationFrames - 1);
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::updatePlaybackFromBackend(
    int currentFrame, int durationFrames, bool isPlaying, bool isPaused)
{
    recordLegacyTimelinePlaybackMutation();
    PlaybackState &playback = activePlaybackState();
    PlaybackState updated = playback;
    updated.durationFrames = std::max(1, durationFrames);
    updated.currentFrame = std::clamp(
        currentFrame, kFirstFrame, updated.durationFrames - 1);
    updated.isPlaying = isPlaying;
    updated.isPaused = isPaused && !isPlaying;

    if (updated.isPlaying == playback.isPlaying
        && updated.isPaused == playback.isPaused
        && updated.currentFrame == playback.currentFrame
        && updated.durationFrames == playback.durationFrames) {
        return;
    }

    playback = updated;
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::updatePlaybackRatePercent(int ratePercent)
{
    recordLegacyTimelinePlaybackMutation();
    const int clampedRate = std::clamp(ratePercent, 50, 200);
    if (sourcePlaybackState_.playbackRatePercent == clampedRate
        && timelinePlaybackState_.playbackRatePercent == clampedRate) {
        return;
    }

    // Preview speed is one transport preference, not two independent source
    // and timeline edit decisions. Preserve it when focus changes contexts.
    sourcePlaybackState_.playbackRatePercent = clampedRate;
    timelinePlaybackState_.playbackRatePercent = clampedRate;
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::leavePausedTimelinePlaybackForEditing()
{
    recordLegacyTimelinePlaybackMutation();
    if (timelinePlaybackState_.isPlaying || !timelinePlaybackState_.isPaused)
        return;

    timelinePlaybackState_.isPaused = false;
    // currentFrame deliberately stays unchanged: selecting an edit target
    // must not silently move the timeline head.
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

void EditorSession::adoptRoutedTimelineTransport(
    const mini_editor::playback_core::TimelineTransportView &view)
{
    PlaybackState adopted = timelinePlaybackState_;
    adopted.isPlaying = view.isPlaying;
    adopted.isPaused = view.isPaused;
    adopted.currentFrame = static_cast<int>(view.timelineFrame);
    adopted.durationFrames = static_cast<int>(view.durationFrames);
    adopted.framesPerSecond = view.framesPerSecond;
    adopted.playbackRatePercent = view.playbackRatePercent;

    if (adopted.isPlaying == timelinePlaybackState_.isPlaying
        && adopted.isPaused == timelinePlaybackState_.isPaused
        && adopted.currentFrame == timelinePlaybackState_.currentFrame
        && adopted.durationFrames == timelinePlaybackState_.durationFrames
        && adopted.framesPerSecond == timelinePlaybackState_.framesPerSecond
        && adopted.playbackRatePercent == timelinePlaybackState_.playbackRatePercent) {
        return;
    }

    timelinePlaybackState_ = adopted;
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::updateTimelineAudioMixState(
    const TimelineAudioMixState &state)
{
    if (timelineAudioMixState_.isVideoTrackMuted == state.isVideoTrackMuted)
        return;

    timelineAudioMixState_ = state;
    projectDirty_ = true;
    // isVideoTrackMuted is carried by the snapshot, so muting is a
    // playback-affecting edit (ADR-006 rule 4).
    synchronizeProjectRuntime();
    notifyStateChanged(EditorChange::AudioMix);
}

void EditorSession::fitTimeline()
{
    timelineViewState_.zoomPercent = 100;
    notifyStateChanged(EditorChange::TimelineView);
}

void EditorSession::restoreWorkspaceState(const TimelineViewState &timelineViewState)
{
    selectedAssetIndex_ = -1;
    selectedTimelineClipId_ = 0;
    isTimelineFocused_ = false;
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
