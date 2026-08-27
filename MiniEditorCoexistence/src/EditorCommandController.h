#pragma once

class EditorSession;
class TimelineEditingController;

// Intent-level commands shared by native MFC menus/accelerators and Qt
// controls. These names deliberately do not contain Win32 command IDs or Qt
// action types, so either UI framework can invoke the same editor behavior.
// A user intent, distinct from EditorHistory's EditorCommand base class.
enum class EditorIntent {
    Undo,
    Redo,
    CopyClip,
    CutClip,
    PasteClip,
    DuplicateClip,
    SplitClip,
    TogglePlayback,
    StopPlayback,
    StepBackward,
    StepForward
};

struct EditorCommandResult {
    bool executed = false;
    // MainFrame owns the native timer. It uses this flag to start or stop the
    // timer after a command changes playback state or stops it incidentally.
    bool playbackTimerNeedsSync = false;
};

class EditorCommandController final
{
public:
    EditorCommandController(EditorSession &session,
                            TimelineEditingController &timelineController);

    bool canExecute(EditorIntent command) const;
    EditorCommandResult execute(EditorIntent command);

private:
    EditorSession &session_;
    TimelineEditingController &timelineController_;
};
