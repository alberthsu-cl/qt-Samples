#include "EditorHistory.h"

#include <utility>

namespace {

void applySelection(EditorCommandContext &context,
                    const EditorSelectionState &selection)
{
    context.selectedAssetIndex = selection.assetIndex;
    context.selectedTimelineClipId = selection.timelineClipId;
    context.isTimelineFocused = selection.isTimelineFocused;
}

void applyTimelineInteraction(EditorCommandContext &context,
                              const TimelineInteractionState &interaction)
{
    applySelection(context, interaction.selection);
    context.sourcePlaybackState = interaction.sourcePlayback;
    context.timelinePlaybackState = interaction.timelinePlayback;
}

} // namespace

SourceClipSettingsCommand::SourceClipSettingsCommand(
    EditorSelectionState selection, ClipSettings before, ClipSettings after)
    : selection_(selection), before_(before), after_(after)
{
}

EditorChange SourceClipSettingsCommand::undo(EditorCommandContext &context)
{
    return apply(context, before_);
}

EditorChange SourceClipSettingsCommand::redo(EditorCommandContext &context)
{
    return apply(context, after_);
}

EditorChange SourceClipSettingsCommand::apply(
    EditorCommandContext &context, const ClipSettings &settings)
{
    context.sourceClipSettings[selection_.assetIndex] = settings;
    applySelection(context, selection_);
    return EditorChange::Selection | EditorChange::ClipSettings;
}

SourceTimelineStateCommand::SourceTimelineStateCommand(
    EditorSelectionState selection, TimelineClipState before,
    TimelineClipState after)
    : selection_(selection), before_(before), after_(after)
{
}

EditorChange SourceTimelineStateCommand::undo(EditorCommandContext &context)
{
    return apply(context, before_);
}

EditorChange SourceTimelineStateCommand::redo(EditorCommandContext &context)
{
    return apply(context, after_);
}

EditorChange SourceTimelineStateCommand::apply(
    EditorCommandContext &context, const TimelineClipState &state)
{
    context.sourceTimelineStates[selection_.assetIndex] = state;
    applySelection(context, selection_);
    return EditorChange::Selection | EditorChange::TimelineClip;
}

TimelineClipSettingsCommand::TimelineClipSettingsCommand(
    EditorSelectionState selection, int clipId,
    ClipSettings before, ClipSettings after)
    : selection_(selection), clipId_(clipId), before_(before), after_(after)
{
}

EditorChange TimelineClipSettingsCommand::undo(EditorCommandContext &context)
{
    return apply(context, before_);
}

EditorChange TimelineClipSettingsCommand::redo(EditorCommandContext &context)
{
    return apply(context, after_);
}

EditorChange TimelineClipSettingsCommand::apply(
    EditorCommandContext &context, const ClipSettings &settings)
{
    context.timelineModel.updateClipSettings(clipId_, settings);
    applySelection(context, selection_);
    return EditorChange::Selection | EditorChange::ClipSettings
        | EditorChange::TimelineClip;
}

TimelineSnapshotCommand::TimelineSnapshotCommand(
    std::vector<TimelineClip> before, std::vector<TimelineClip> after,
    TimelineInteractionState interactionBefore,
    TimelineInteractionState interactionAfter)
    : before_(std::move(before))
    , after_(std::move(after))
    , interactionBefore_(interactionBefore)
    , interactionAfter_(interactionAfter)
{
}

EditorChange TimelineSnapshotCommand::undo(EditorCommandContext &context)
{
    return apply(context, before_, interactionBefore_);
}

EditorChange TimelineSnapshotCommand::redo(EditorCommandContext &context)
{
    return apply(context, after_, interactionAfter_);
}

EditorChange TimelineSnapshotCommand::apply(
    EditorCommandContext &context, const std::vector<TimelineClip> &clips,
    const TimelineInteractionState &interaction)
{
    context.timelineModel.replaceClips(clips);
    applyTimelineInteraction(context, interaction);
    return EditorChange::Selection | EditorChange::TimelineClip
        | EditorChange::Playback;
}

bool EditorHistory::canUndo() const
{
    return !undoCommands_.empty();
}

bool EditorHistory::canRedo() const
{
    return !redoCommands_.empty();
}

void EditorHistory::record(std::unique_ptr<EditorCommand> command)
{
    if (!command)
        return;
    undoCommands_.push_back(std::move(command));
    redoCommands_.clear();
}

EditorChange EditorHistory::undo(EditorCommandContext &context)
{
    if (!canUndo())
        return EditorChange::None;

    std::unique_ptr<EditorCommand> command = std::move(undoCommands_.back());
    undoCommands_.pop_back();
    const EditorChange changes = command->undo(context);
    redoCommands_.push_back(std::move(command));
    return changes;
}

EditorChange EditorHistory::redo(EditorCommandContext &context)
{
    if (!canRedo())
        return EditorChange::None;

    std::unique_ptr<EditorCommand> command = std::move(redoCommands_.back());
    redoCommands_.pop_back();
    const EditorChange changes = command->redo(context);
    undoCommands_.push_back(std::move(command));
    return changes;
}

void EditorHistory::clear()
{
    undoCommands_.clear();
    redoCommands_.clear();
}
