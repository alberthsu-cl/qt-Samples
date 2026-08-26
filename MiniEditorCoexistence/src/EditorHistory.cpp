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
    EditorSelectionState selectionBefore,
    EditorSelectionState selectionAfter)
    : before_(std::move(before))
    , after_(std::move(after))
    , selectionBefore_(selectionBefore)
    , selectionAfter_(selectionAfter)
{
}

EditorChange TimelineSnapshotCommand::undo(EditorCommandContext &context)
{
    return apply(context, before_, selectionBefore_);
}

EditorChange TimelineSnapshotCommand::redo(EditorCommandContext &context)
{
    return apply(context, after_, selectionAfter_);
}

EditorChange TimelineSnapshotCommand::apply(
    EditorCommandContext &context, const std::vector<TimelineClip> &clips,
    const EditorSelectionState &selection)
{
    context.timelineModel.replaceClips(clips);
    applySelection(context, selection);
    return EditorChange::Selection | EditorChange::TimelineClip;
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
