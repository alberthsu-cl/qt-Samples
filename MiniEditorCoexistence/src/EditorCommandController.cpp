#include "EditorCommandController.h"

#include "EditorSession.h"
#include "PlaybackBackend.h"
#include "TimelineEditingController.h"

EditorCommandController::EditorCommandController(
    EditorSession &session, TimelineEditingController &timelineController,
    IPlaybackBackend &playbackBackend)
    : session_(session)
    , timelineController_(timelineController)
    , playbackBackend_(playbackBackend)
{
}

bool EditorCommandController::canExecute(EditorIntent command) const
{
    switch (command) {
    case EditorIntent::Undo:
        return session_.canUndo();
    case EditorIntent::Redo:
        return session_.canRedo();
    case EditorIntent::CopyClip:
        return timelineController_.canCopy();
    case EditorIntent::CutClip:
        return timelineController_.canCut();
    case EditorIntent::PasteClip:
        return timelineController_.canPaste();
    case EditorIntent::DuplicateClip:
        return timelineController_.canDuplicate();
    case EditorIntent::SplitClip:
        return timelineController_.canSplitAtHead();
    case EditorIntent::TogglePlayback:
    case EditorIntent::StopPlayback:
    case EditorIntent::StepBackward:
    case EditorIntent::StepForward:
        return true;
    }

    return false;
}

EditorCommandResult EditorCommandController::execute(EditorIntent command)
{
    if (!canExecute(command))
        return {};

    switch (command) {
    case EditorIntent::Undo:
        return { session_.undo(), false };
    case EditorIntent::Redo:
        return { session_.redo(), false };
    case EditorIntent::CopyClip:
        return { timelineController_.copy(), false };
    case EditorIntent::CutClip:
        return { timelineController_.cut(), true };
    case EditorIntent::PasteClip:
        return { timelineController_.paste(), true };
    case EditorIntent::DuplicateClip:
        return { timelineController_.duplicate(), true };
    case EditorIntent::SplitClip:
        return { timelineController_.splitAtHead(), true };
    case EditorIntent::TogglePlayback:
        playbackBackend_.executeCommand(LegacyPlaybackCommand::TogglePlayPause);
        return { true, true };
    case EditorIntent::StopPlayback:
        playbackBackend_.executeCommand(LegacyPlaybackCommand::Stop);
        return { true, true };
    case EditorIntent::StepBackward:
        playbackBackend_.executeCommand(LegacyPlaybackCommand::StepBackward);
        return { true, true };
    case EditorIntent::StepForward:
        playbackBackend_.executeCommand(LegacyPlaybackCommand::StepForward);
        return { true, true };
    }

    return {};
}
