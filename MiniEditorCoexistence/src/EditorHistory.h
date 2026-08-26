#pragma once

#include "EditorChange.h"
#include "ProjectState.h"
#include "TimelineModel.h"

#include <memory>
#include <vector>

struct EditorSelectionState {
    int assetIndex = 0;
    int timelineClipId = 0;
    bool isTimelineFocused = false;
};

// References to the state that commands are allowed to restore. Commands do
// not know about EditorSession, MFC, or Qt; the session supplies this narrow
// context only while Undo or Redo is running.
struct EditorCommandContext {
    std::vector<ClipSettings> &sourceClipSettings;
    std::vector<TimelineClipState> &sourceTimelineStates;
    TimelineModel &timelineModel;
    int &selectedAssetIndex;
    int &selectedTimelineClipId;
    bool &isTimelineFocused;
};

class EditorCommand
{
public:
    virtual ~EditorCommand() = default;
    virtual EditorChange undo(EditorCommandContext &context) = 0;
    virtual EditorChange redo(EditorCommandContext &context) = 0;
};

class SourceClipSettingsCommand final : public EditorCommand
{
public:
    SourceClipSettingsCommand(EditorSelectionState selection,
                              ClipSettings before, ClipSettings after);
    EditorChange undo(EditorCommandContext &context) override;
    EditorChange redo(EditorCommandContext &context) override;

private:
    EditorChange apply(EditorCommandContext &context,
                       const ClipSettings &settings);
    EditorSelectionState selection_;
    ClipSettings before_;
    ClipSettings after_;
};

class SourceTimelineStateCommand final : public EditorCommand
{
public:
    SourceTimelineStateCommand(EditorSelectionState selection,
                               TimelineClipState before,
                               TimelineClipState after);
    EditorChange undo(EditorCommandContext &context) override;
    EditorChange redo(EditorCommandContext &context) override;

private:
    EditorChange apply(EditorCommandContext &context,
                       const TimelineClipState &state);
    EditorSelectionState selection_;
    TimelineClipState before_;
    TimelineClipState after_;
};

class TimelineClipSettingsCommand final : public EditorCommand
{
public:
    TimelineClipSettingsCommand(EditorSelectionState selection, int clipId,
                                ClipSettings before, ClipSettings after);
    EditorChange undo(EditorCommandContext &context) override;
    EditorChange redo(EditorCommandContext &context) override;

private:
    EditorChange apply(EditorCommandContext &context,
                       const ClipSettings &settings);
    EditorSelectionState selection_;
    int clipId_ = 0;
    ClipSettings before_;
    ClipSettings after_;
};

// Add, move, trim, ripple, delete, and split all become the same history
// concept: one valid timeline before the command and one valid timeline after
// it. This keeps compound edits atomic without teaching EditorHistory about
// individual editing features.
class TimelineSnapshotCommand final : public EditorCommand
{
public:
    TimelineSnapshotCommand(std::vector<TimelineClip> before,
                            std::vector<TimelineClip> after,
                            EditorSelectionState selectionBefore,
                            EditorSelectionState selectionAfter);
    EditorChange undo(EditorCommandContext &context) override;
    EditorChange redo(EditorCommandContext &context) override;

private:
    EditorChange apply(EditorCommandContext &context,
                       const std::vector<TimelineClip> &clips,
                       const EditorSelectionState &selection);
    std::vector<TimelineClip> before_;
    std::vector<TimelineClip> after_;
    EditorSelectionState selectionBefore_;
    EditorSelectionState selectionAfter_;
};

class EditorHistory final
{
public:
    bool canUndo() const;
    bool canRedo() const;
    void record(std::unique_ptr<EditorCommand> command);
    EditorChange undo(EditorCommandContext &context);
    EditorChange redo(EditorCommandContext &context);
    void clear();

private:
    std::vector<std::unique_ptr<EditorCommand>> undoCommands_;
    std::vector<std::unique_ptr<EditorCommand>> redoCommands_;
};
