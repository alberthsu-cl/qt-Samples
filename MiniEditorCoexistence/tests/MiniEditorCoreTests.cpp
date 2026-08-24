#include "EditorSession.h"
#include "ProjectSerializer.h"
#include "WorkspaceLayout.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int right(const WorkspaceRect &rect)
{
    return rect.left + rect.width;
}

int bottom(const WorkspaceRect &rect)
{
    return rect.top + rect.height;
}

void editorSessionOwnsAndClampsState()
{
    EditorSession session(6);
    std::vector<EditorChange> notifications;
    const EditorSession::ObserverId observerId = session.addObserver(
        [&notifications](EditorChange changes) { notifications.push_back(changes); });

    session.selectAsset(99);
    require(session.selectedAssetIndex() == 5, "Selection must clamp to the last asset.");

    ClipSettings settings;
    settings.opacityPercent = 65;
    settings.scalePercent = 125;
    session.updateSelectedClipSettings(settings);
    require(session.selectedClipSettings().opacityPercent == 65,
            "Session must keep the selected clip settings.");

    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    require(session.playbackState().isPlaying, "Play command must start playback.");
    session.advancePlaybackFrame();
    require(session.playbackState().currentFrame == 1,
            "Playing session must advance one frame.");

    session.handlePlaybackCommand(PlaybackCommand::StepForward);
    require(!session.playbackState().isPlaying, "Frame stepping must stop playback.");
    require(session.playbackState().currentFrame == 2, "Frame step must advance one frame.");

    session.seekTimeline(999);
    require(session.playbackState().currentFrame == 299, "Seek must clamp to the last frame.");

    TimelineViewState timelineViewState;
    timelineViewState.zoomPercent = 999;
    timelineViewState.isAudioTrackVisible = false;
    session.updateTimelineViewState(timelineViewState);
    require(session.timelineViewState().zoomPercent == 200, "Zoom must clamp to 200%.");
    require(!session.timelineViewState().isAudioTrackVisible,
            "Session must retain audio-track visibility.");
    require(notifications.size() == 7, "Every user-visible state change must notify once.");
    require(notifications[0] == EditorChange::Selection,
            "Selection must report its typed change category.");
    require(notifications[2] == EditorChange::Playback,
            "Playback commands must report a playback change.");
    require(notifications[6] == EditorChange::TimelineView,
            "Zoom changes must report a timeline-view change.");

    session.removeObserver(observerId);
    session.selectAsset(0);
    require(notifications.size() == 7, "Removed observer must no longer receive notifications.");
}

void editorSessionUndoRedoTracksOnlyClipEdits()
{
    EditorSession session(2);
    session.selectAsset(1);
    require(!session.canUndo(), "Selection alone must not create undo history.");

    ClipSettings editedSettings = session.selectedClipSettings();
    editedSettings.opacityPercent = 40;
    editedSettings.scalePercent = 140;
    session.updateSelectedClipSettings(editedSettings);
    require(session.canUndo(), "A clip-settings edit must become undoable.");
    require(!session.canRedo(), "A new edit must clear redo history.");

    session.selectAsset(0);
    require(session.undo(), "Undo must restore the previous clip settings.");
    require(session.selectedAssetIndex() == 1,
            "Undo must select the asset that owns the restored edit.");
    require(session.selectedClipSettings().opacityPercent == 100,
            "Undo must restore the original opacity.");
    require(session.selectedClipSettings().scalePercent == 100,
            "Undo must restore the original scale.");
    require(session.canRedo(), "Undo must make the command redoable.");

    require(session.redo(), "Redo must reapply the clip settings.");
    require(session.selectedClipSettings().opacityPercent == 40,
            "Redo must restore the edited opacity.");
    require(session.selectedClipSettings().scalePercent == 140,
            "Redo must restore the edited scale.");

    TimelineViewState timelineViewState;
    timelineViewState.zoomPercent = 150;
    session.updateTimelineViewState(timelineViewState);
    require(session.canUndo(), "Timeline-view changes must not alter clip edit history.");
    require(session.undo(), "The clip edit must remain undoable after a view change.");
    require(session.selectedClipSettings().opacityPercent == 100,
            "Undo must still target the clip edit, not timeline view state.");
}

void editorSessionUndoRedoTracksTimelineClipMoves()
{
    EditorSession session(2);
    session.selectAsset(1);

    TimelineClipState movedClip = session.selectedTimelineClipState();
    movedClip.startFrame = 120;
    session.updateSelectedTimelineClipState(movedClip);
    require(session.canUndo(), "A timeline clip move must become undoable.");
    require(session.selectedTimelineClipState().startFrame == 120,
            "Timeline move must update the selected clip start frame.");

    session.selectAsset(0);
    require(session.undo(), "Undo must restore the previous timeline clip position.");
    require(session.selectedAssetIndex() == 1,
            "Undo must select the asset whose timeline clip changed.");
    require(session.selectedTimelineClipState().startFrame == 0,
            "Undo must restore the original clip start frame.");

    require(session.redo(), "Redo must reapply a timeline clip move.");
    require(session.selectedTimelineClipState().startFrame == 120,
            "Redo must restore the moved clip start frame.");

    movedClip.startFrame = 999;
    movedClip.durationFrames = 300;
    session.updateSelectedTimelineClipState(movedClip);
    require(session.selectedTimelineClipState().startFrame == 300,
            "Timeline clip start must clamp within the editable timeline range.");
}

void projectDocumentRoundTripsEditState()
{
    EditorSession sourceSession(2);
    sourceSession.selectAsset(1);

    ClipSettings settings = sourceSession.selectedClipSettings();
    settings.opacityPercent = 55;
    settings.position = ClipPosition::BottomRight;
    sourceSession.updateSelectedClipSettings(settings);
    require(sourceSession.isProjectDirty(), "An edit must mark the project dirty.");
    TimelineClipState timelineClip = sourceSession.selectedTimelineClipState();
    timelineClip.startFrame = 240;
    sourceSession.updateSelectedTimelineClipState(timelineClip);

    const std::filesystem::path testPath = std::filesystem::temp_directory_path()
        / "MiniEditorCoreTests.mini-editor.json";
    std::wstring errorMessage;
    require(ProjectSerializer::save(testPath, sourceSession.projectSnapshot(), &errorMessage),
            "Project serializer must save a valid project.");

    const auto loadedProject = ProjectSerializer::load(testPath, 2, &errorMessage);
    std::error_code removeError;
    std::filesystem::remove(testPath, removeError);
    require(loadedProject.has_value(), "Project serializer must load its saved project.");

    EditorSession destinationSession(2);
    destinationSession.replaceProject(*loadedProject);
    destinationSession.selectAsset(1);
    require(destinationSession.selectedClipSettings().opacityPercent == 55,
            "Loaded project must restore clip settings.");
    require(destinationSession.selectedClipSettings().position == ClipPosition::BottomRight,
            "Loaded project must restore clip position.");
    require(destinationSession.selectedTimelineClipState().startFrame == 240,
            "Loaded project must restore timeline position.");
    require(!destinationSession.canUndo(), "Loading a project must begin with clean edit history.");
    require(!destinationSession.isProjectDirty(), "Loading a project must clear dirty state.");
    destinationSession.markProjectSaved();
    require(!destinationSession.isProjectDirty(), "Marking a project saved must clear dirty state.");
}

void workspaceLayoutProtectsPaneBounds()
{
    WorkspaceLayout layout;
    WorkspaceGeometry geometry = layout.calculate(1500, 900);

    require(geometry.mediaLibrary.width >= 180, "Media library must respect its minimum width.");
    require(geometry.properties.width >= 190, "Properties must respect its minimum width.");
    require(geometry.previewCanvas.width >= 320, "Preview must respect its minimum width.");
    require(right(geometry.mediaLibrary) <= geometry.leftSplitter.left,
            "Left splitter must follow the media library.");
    require(right(geometry.previewCanvas) <= geometry.rightSplitter.left,
            "Right splitter must follow the preview canvas.");
    require(bottom(geometry.previewCanvas) <= geometry.transport.top,
            "Transport must remain below the preview canvas.");
    require(geometry.timeline.top >= bottom(geometry.transport),
            "Timeline must remain below the transport region.");

    layout.moveLeftSplitter(500);
    layout.moveRightSplitter(1050, 1500);
    layout.moveTimelineSplitter(620, 900);
    geometry = layout.calculate(1500, 900);

    const WorkspaceLayoutState state = layout.state();
    require(state.mediaLibraryWidth == 494, "Left splitter must update media width.");
    require(state.propertiesWidth == 438, "Right splitter must update properties width.");
    require(state.timelineHeight == 274, "Timeline splitter must update timeline height.");
    require(geometry.timelineCanvas.height >= 0, "Timeline canvas bounds must stay valid.");
}

} // namespace

int main()
{
    try {
        editorSessionOwnsAndClampsState();
        editorSessionUndoRedoTracksOnlyClipEdits();
        editorSessionUndoRedoTracksTimelineClipMoves();
        projectDocumentRoundTripsEditState();
        workspaceLayoutProtectsPaneBounds();
        std::cout << "MiniEditorCoreTests passed.\n";
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "MiniEditorCoreTests failed: " << exception.what() << '\n';
        return 1;
    }
}
