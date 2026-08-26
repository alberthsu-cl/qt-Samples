#include "EditorSession.h"
#include "ProjectSerializer.h"
#include "MediaLibrary.h"
#include "TimelineModel.h"
#include "TimelineClipEdit.h"
#include "TimelineTrackPolicy.h"
#include "TimelineGeometry.h"
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
    const int insertedTimelineClipId = sourceSession.addTimelineClip(
        1, TimelineTrackType::Video, 120, 300);
    const int insertedAudioClipId = sourceSession.addTimelineClip(
        2, TimelineTrackType::Audio, 500, 4170);
    sourceSession.selectTimelineClip(insertedTimelineClipId);
    sourceSession.updateSelectedClipSettings(settings);
    require(sourceSession.moveTimelineClip(insertedTimelineClipId,
                                           { 120, 240, 30 }),
            "Source-aware placement must accept a valid video source-in frame.");

    const std::filesystem::path testPath = std::filesystem::temp_directory_path()
        / "MiniEditorCoreTests.mini-editor.json";
    EditorProject sourceProject = sourceSession.projectSnapshot();
    sourceProject.mediaAssets = {
        { 1, L"D:/media/first.mp4", L"first.mp4", MediaKind::Video, 300, 0x5078A0 },
        { 2, L"D:/media/second.mp3", L"second.mp3", MediaKind::Audio, 4170, 0x2878B4 }
    };
    std::wstring errorMessage;
    require(ProjectSerializer::save(testPath, sourceProject, &errorMessage),
            "Project serializer must save a valid project.");

    const auto loadedProject = ProjectSerializer::load(testPath, 2, &errorMessage);
    std::error_code removeError;
    std::filesystem::remove(testPath, removeError);
    require(loadedProject.has_value(), "Project serializer must load its saved project.");
    require(loadedProject->mediaAssets.size() == 2
                && loadedProject->mediaAssets[1].kind == MediaKind::Audio,
            "Version 6 must restore the project media library.");

    EditorSession destinationSession(2);
    destinationSession.replaceProject(*loadedProject);
    destinationSession.selectTimelineClip(insertedTimelineClipId);
    require(destinationSession.selectedClipSettings().opacityPercent == 55,
            "Loaded project must restore timeline placement settings.");
    require(destinationSession.selectedClipSettings().position == ClipPosition::BottomRight,
            "Loaded project must restore clip position.");
    const TimelineClip *loadedTimelineClip =
        destinationSession.timelineModel().findClip(insertedTimelineClipId);
    require(loadedTimelineClip != nullptr
                && loadedTimelineClip->state.startFrame == 120
                && loadedTimelineClip->state.durationFrames == 240
                && loadedTimelineClip->state.sourceInFrame == 30,
            "Loaded project must restore the complete source-aware placement.");
    const TimelineClip *loadedAudioClip =
        destinationSession.timelineModel().findClip(insertedAudioClipId);
    require(loadedAudioClip != nullptr
                && loadedAudioClip->trackType == TimelineTrackType::Audio
                && loadedAudioClip->state.durationFrames == 4170,
            "Loaded project must restore timeline track and duration.");
    require(!destinationSession.canUndo(), "Loading a project must begin with clean edit history.");
    require(!destinationSession.isProjectDirty(), "Loading a project must clear dirty state.");
    destinationSession.markProjectSaved();
    require(!destinationSession.isProjectDirty(), "Marking a project saved must clear dirty state.");
}

void version5ProjectMigratesToSourceInZero()
{
    const std::filesystem::path testPath = std::filesystem::temp_directory_path()
        / "MiniEditorCoreTests-v5.mini-editor.json";
    {
        std::ofstream output(testPath, std::ios::trunc);
        output << R"({
  "formatVersion": 5,
  "mediaAssets": [
    { "id": 1, "filePath": "D:/media/legacy.mp4", "displayName": "legacy.mp4", "kind": "Video", "durationFrames": 300, "thumbnailColorRgb": 5273760 }
  ],
  "timelineClips": [
    { "id": 1, "mediaAssetId": 1, "trackType": "Video", "startFrame": 30, "durationFrames": 120, "opacityPercent": 100, "scalePercent": 100, "position": "Center" }
  ]
})";
    }

    std::wstring errorMessage;
    const auto project = ProjectSerializer::load(testPath, 1, &errorMessage);
    std::error_code removeError;
    std::filesystem::remove(testPath, removeError);
    require(project.has_value() && project->timelineItems.size() == 1,
            "Version 5 project must remain loadable after the v6 migration.");
    require(project->timelineItems.front().state.sourceInFrame == 0,
            "A version 5 placement must migrate with source-in frame zero.");
}

void projectSerializerValidatesTimedSourcesButAllowsStillDuration()
{
    const std::filesystem::path testPath = std::filesystem::temp_directory_path()
        / "MiniEditorCoreTests-source-range.mini-editor.json";
    EditorProject project = EditorProject::createDefault(2);
    project.mediaAssets = {
        { 1, L"D:/media/video.mp4", L"video.mp4", MediaKind::Video, 100, 0x5078A0 },
        { 2, L"D:/media/still.png", L"still.png", MediaKind::Image, 90, 0x2878B4 }
    };
    project.timelineItems = {
        { 1, 1, TimelineTrackType::Video, { 0, 60, 50 }, {} }
    };

    std::wstring errorMessage;
    require(!ProjectSerializer::save(testPath, project, &errorMessage),
            "Serializer must reject a video range beyond its source duration.");

    project.timelineItems = {
        { 2, 2, TimelineTrackType::Video, { 0, 300, 0 }, {} }
    };
    require(ProjectSerializer::save(testPath, project, &errorMessage),
            "Serializer must allow a still image to have a longer display duration.");
    const auto loaded = ProjectSerializer::load(testPath, 2, &errorMessage);
    std::error_code removeError;
    std::filesystem::remove(testPath, removeError);
    require(loaded.has_value()
                && loaded->timelineItems.front().state.durationFrames == 300,
            "Still-image display duration must survive project round-trip.");
}

void timelineModelStartsEmptyAndOwnsIndependentClipIds()
{
    TimelineModel timeline;
    require(timeline.clips().empty(), "A new timeline must start empty.");
    require(timeline.contentDurationFrames() == 0,
            "An empty timeline must have no playable content duration.");

    const int firstClipId = timeline.addClip(2, TimelineTrackType::Video);
    const int secondClipId = timeline.addClip(3, TimelineTrackType::Audio, { 180, 90 });
    require(firstClipId != secondClipId, "Each timeline placement needs a unique ID.");
    require(timeline.clips().size() == 2, "Timeline must retain multiple placements.");
    require(timeline.findClip(firstClipId)->mediaAssetId == 2,
            "A timeline clip must reference its source media asset.");
    require(timeline.findClip(secondClipId)->trackType == TimelineTrackType::Audio,
            "Audio placements must target the audio track.");
    require(timeline.durationFrames() == 600,
            "A new timeline must retain its minimum duration.");
    require(timeline.contentDurationFrames() == 270,
            "Playable duration must end at the final media item, not the canvas minimum.");

    require(timeline.moveClip(secondClipId, { 240, 120 }),
            "Timeline must move an existing clip by ID.");
    require(timeline.findClip(secondClipId)->state.startFrame == 240,
            "Moving a clip must update only that placement.");
    require(timeline.durationFrames() == 600,
            "Short clips must not shrink the timeline below its minimum.");
    require(timeline.moveClip(secondClipId, { 700, 120 }),
            "Timeline must allow a clip beyond the initial duration.");
    require(timeline.durationFrames() == 820,
            "Timeline duration must extend to the furthest clip end.");
    require(timeline.contentDurationFrames() == 820,
            "Playable duration must follow the final media item after editing.");
    require(timeline.removeClip(firstClipId), "Timeline must remove a clip by ID.");
    require(timeline.clips().size() == 1, "Removing one clip must preserve the other.");
    timeline.clear();
    require(timeline.clips().empty(), "Clearing a timeline must remove all placements.");
}

void editorSessionUndoRedoTracksModelClipMoves()
{
    EditorSession session(2);
    const int clipId = session.addTimelineClip(1, TimelineTrackType::Video, 0);
    require(session.moveTimelineClip(clipId, { 150, 120 }),
            "Moving or trimming a model clip must succeed through EditorSession.");
    require(session.canUndo(), "A model clip edit must become undoable.");
    require(session.timelineModel().findClip(clipId)->state.startFrame == 150,
            "Session must retain the moved model clip.");
    require(session.timelineModel().findClip(clipId)->state.durationFrames == 120,
            "Session must retain the trimmed model clip duration.");
    require(session.undo(), "Undo must restore a model clip edit.");
    const TimelineClip *undoClip = session.timelineModel().findClip(clipId);
    require(undoClip->state.startFrame == 0 && undoClip->state.durationFrames == 180,
            "Undo must restore the original model clip range.");
    require(session.redo(), "Redo must reapply a model clip edit.");
    const TimelineClip *redoClip = session.timelineModel().findClip(clipId);
    require(redoClip->state.startFrame == 150 && redoClip->state.durationFrames == 120,
            "Redo must restore the edited model clip range.");
}

void editorSessionUndoRedoTracksLibraryInsertion()
{
    EditorSession session(2);
    const int clipId = session.addTimelineClip(2, TimelineTrackType::Video, 60);
    require(clipId > 0, "Adding a library item must create a timeline clip.");
    require(session.timelineModel().clips().size() == 1,
            "The inserted clip must appear in the timeline model.");
    require(session.undo(), "Undo must remove a library insertion.");
    require(session.timelineModel().clips().empty(),
            "Undo must remove the inserted timeline clip.");
    require(session.redo(), "Redo must restore a library insertion.");
    require(session.timelineModel().findClip(clipId) != nullptr,
            "Redo must restore the same timeline clip ID.");
}

void editorSessionUsesRequestedTimelineClipDuration()
{
    EditorSession session(2);
    const int clipId = session.addTimelineClip(1, TimelineTrackType::Audio, 0, 4170);
    require(session.timelineModel().findClip(clipId)->state.durationFrames == 4170,
            "Session must retain an asset's timeline duration.");
    require(session.timelineModel().durationFrames() == 4170,
            "A long audio clip must extend the timeline duration.");
    require(session.moveTimelineClip(clipId, { 600, 4170 }),
            "A long clip must remain movable beyond the legacy timeline range.");
    require(session.timelineModel().findClip(clipId)->state.durationFrames == 4170,
            "Moving a long clip must not truncate its duration.");
    require(session.timelineModel().durationFrames() == 4770,
            "Moving a long clip must extend the project duration dynamically.");
}

void editorSessionUndoRedoTracksClipDeletion()
{
    EditorSession session(2);
    const int clipId = session.addTimelineClip(1, TimelineTrackType::Video, 30);
    require(session.removeTimelineClip(clipId), "Deleting a timeline clip must succeed.");
    require(session.timelineModel().clips().empty(),
            "Deleting a timeline clip must remove it from the model.");
    require(session.undo(), "Undo must restore a deleted timeline clip.");
    require(session.timelineModel().findClip(clipId) != nullptr,
            "Undo must restore the deleted clip identity.");
    require(session.redo(), "Redo must delete the timeline clip again.");
    require(session.timelineModel().findClip(clipId) == nullptr,
            "Redo must remove the deleted clip again.");
}

void focusedTimelineClipOwnsIndependentPlacementSettings()
{
    EditorSession session(1);
    const int firstClipId = session.addTimelineClip(1, TimelineTrackType::Video, 0);
    const int secondClipId = session.addTimelineClip(1, TimelineTrackType::Video, 300);

    session.selectTimelineClip(firstClipId);
    session.updateSelectedClipSettings({ 45, 150, ClipPosition::TopLeft });
    require(session.timelineModel().findClip(firstClipId)->settings.opacityPercent == 45,
            "Properties edits must update the focused timeline placement.");
    require(session.timelineModel().findClip(secondClipId)->settings.opacityPercent == 100,
            "Two placements of one source asset must keep independent settings.");
    require(session.undo(), "Timeline placement settings must be undoable.");
    require(session.timelineModel().findClip(firstClipId)->settings.opacityPercent == 100,
            "Undo must restore the focused placement settings.");
    require(session.redo(), "Timeline placement settings must be redoable.");
    require(session.timelineModel().findClip(firstClipId)->settings.scalePercent == 150,
            "Redo must restore the focused placement settings.");

    session.selectTimelineClip(secondClipId);
    session.updateSelectedClipSettings({ 70, 80, ClipPosition::BottomRight });
    require(session.timelineModel().findClip(secondClipId)->settings.opacityPercent == 70,
            "Properties edits must also update a later focused placement.");
    require(session.timelineModel().findClip(firstClipId)->settings.opacityPercent == 45,
            "Editing a later placement must not modify the first placement.");
}

void playbackStopsAtFocusedPreviewDuration()
{
    EditorSession session(1);
    session.setPlaybackDuration(3, true);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    session.advancePlaybackFrame();
    session.advancePlaybackFrame();
    session.advancePlaybackFrame();
    require(!session.playbackState().isPlaying,
            "Preview playback must stop when its focused duration is reached.");
    require(session.playbackState().currentFrame == 2,
            "Stopped preview playback must remain on its final valid frame.");
}

void timelineSlotResolutionReturnsNothingForGaps()
{
    TimelineModel timeline;
    const int firstId = timeline.addClip(1, TimelineTrackType::Video, { 0, 90 });
    const int secondId = timeline.addClip(2, TimelineTrackType::Video, { 120, 60 });
    require(timeline.visibleVideoClipAt(89)->id == firstId,
            "Timeline preview must resolve the first active media slot.");
    require(timeline.visibleVideoClipAt(90) == nullptr,
            "Timeline preview must be empty when the playhead is in a gap.");
    require(timeline.visibleVideoClipAt(120)->id == secondId,
            "Timeline preview must switch to the next active media slot.");
}

void singleTrackPolicyPreventsOverlapAndFindsNearestGap()
{
    TimelineModel timeline;
    const int firstId = timeline.addClip(1, TimelineTrackType::Video, { 0, 180 });
    const int secondId = timeline.addClip(2, TimelineTrackType::Video, { 240, 120 });
    require(firstId > 0 && secondId > 0,
            "Non-overlapping clips must be accepted on the single video track.");
    require(timeline.addClip(3, TimelineTrackType::Video, { 90, 180 }) == 0,
            "The model must reject an overlapping clip on V1.");
    require(timeline.addClip(4, TimelineTrackType::Audio, { 90, 180 }) > 0,
            "The same time range must remain available on the independent A1 track.");
    require(!timeline.moveClip(secondId, { 120, 120 }),
            "The model must reject a move that overlaps another V1 clip.");

    require(TimelineTrackPolicy::nearestAvailableStart(
                timeline.clips(), TimelineTrackType::Video, 150, 60) == 180,
            "A dropped clip must snap to the nearest gap boundary.");
    require(TimelineTrackPolicy::nearestAvailableStart(
                timeline.clips(), TimelineTrackType::Video, 270, 60) == 180,
            "Equal-distance gaps must choose the earlier edit position.");

    const TimelineClip *secondClip = timeline.findClip(secondId);
    const TimelineClipState startTrim = TimelineTrackPolicy::constrainStartTrim(
        timeline.clips(), *secondClip, { 120, 240, 0 }, MediaKind::Video);
    require(startTrim.startFrame == 180 && startTrim.durationFrames == 180,
            "A left trim must stop at the previous clip's end.");
    const TimelineClip *firstClip = timeline.findClip(firstId);
    const TimelineClipState endTrim = TimelineTrackPolicy::constrainEndTrim(
        timeline.clips(), *firstClip, { 0, 300, 0 });
    require(endTrim.durationFrames == 240,
            "A right trim must stop at the next clip's start.");

    EditorSession session(2);
    require(session.addTimelineClip(1, TimelineTrackType::Video, 0, 180) > 0,
            "The session must insert the first V1 clip.");
    const int snappedId = session.addTimelineClip(
        2, TimelineTrackType::Video, 90, 180);
    require(session.timelineModel().findClip(snappedId)->state.startFrame == 180,
            "A session insertion must store the policy's non-overlapping position.");
    require(session.undo() && session.redo(),
            "A snapped insertion must remain a single undoable command.");
    require(session.timelineModel().findClip(snappedId)->state.startFrame == 180,
            "Redo must restore the same snapped position.");
}

void timelineGeometryOwnsFrameworkNeutralCoordinatesAndHitTesting()
{
    const TimelineGeometry geometry(100, 600);
    require(geometry.xForFrame(0) == TimelineGeometry::kTimelineLeft,
            "Frame zero must start after the track labels.");
    require(geometry.xForFrame(300) == 410,
            "One scale unit must use the configured pixel width.");
    require(geometry.frameAtX(410) == 300,
            "Timeline pixel conversion must return the matching frame.");
    require(geometry.rulerFrameAtX(geometry.xForFrame(450)) == 450,
            "Ruler seeking must work beyond the first scale unit.");

    const TimelineRectangle videoTrack = geometry.trackRectangle(
        TimelineTrackType::Video, 1000);
    const TimelineRectangle audioTrack = geometry.trackRectangle(
        TimelineTrackType::Audio, 1000);
    require(videoTrack.top == TimelineGeometry::kRulerHeight,
            "V1 must begin directly below the ruler.");
    require(audioTrack.top == videoTrack.top + videoTrack.height
                                  + TimelineGeometry::kTrackGap,
            "A1 must follow V1 with the configured gap.");

    const std::vector<TimelineClip> clips{
        { 1, 1, TimelineTrackType::Video, { 0, 180 }, {} },
        { 2, 2, TimelineTrackType::Video, { 90, 180 }, {} }
    };
    const TimelinePoint overlapPoint{
        geometry.xForFrame(120),
        videoTrack.top + TimelineGeometry::kClipVerticalMargin + 1
    };
    const TimelineClip *topmost = geometry.topmostClipAt(clips, overlapPoint);
    require(topmost != nullptr && topmost->id == 2,
            "Geometry hit-testing must select the later painted overlap.");

    const TimelinePoint firstOnlyPoint{
        geometry.xForFrame(30),
        videoTrack.top + TimelineGeometry::kClipVerticalMargin + 1
    };
    const TimelineClip *firstOnly = geometry.topmostClipAt(
        clips, firstOnlyPoint);
    require(firstOnly != nullptr && firstOnly->id == 1,
            "Geometry hit-testing must retain the first non-overlapping clip.");
}

void timelineClipTrimmingPreservesValidRangesAndHandleHits()
{
    const TimelineTrimContext videoContext{ MediaKind::Video, 400 };
    const TimelineClipState videoOriginal{ 100, 200, 50 };
    const TimelineClipState moved = TimelineClipEdit::moveTo(videoOriginal, -20);
    require(moved.startFrame == 0 && moved.durationFrames == 200
                && moved.sourceInFrame == 50,
            "Moving must preserve duration and source-in while preventing a negative start.");

    const TimelineClipState startTrim = TimelineClipEdit::trimStartTo(
        videoOriginal, 140, videoContext);
    require(startTrim.startFrame == 140 && startTrim.durationFrames == 160
                && startTrim.sourceInFrame == 90,
            "Video start trim must advance source-in and preserve the timeline end.");
    const TimelineClipState shortestStartTrim = TimelineClipEdit::trimStartTo(
        videoOriginal, 999, videoContext);
    require(shortestStartTrim.startFrame == 299
                && shortestStartTrim.durationFrames == 1
                && shortestStartTrim.sourceInFrame == 249,
            "Video start trim must preserve at least one source frame.");
    const TimelineClipState outwardStartTrim = TimelineClipEdit::trimStartTo(
        videoOriginal, 0, videoContext);
    require(outwardStartTrim.startFrame == 50
                && outwardStartTrim.durationFrames == 250
                && outwardStartTrim.sourceInFrame == 0,
            "Video start untrim must stop at source frame zero.");

    const TimelineClipState endTrim = TimelineClipEdit::trimEndTo(
        videoOriginal, 250, videoContext);
    require(endTrim.startFrame == 100 && endTrim.durationFrames == 150
                && endTrim.sourceInFrame == 50,
            "Video end trim must preserve timeline start and source-in.");
    const TimelineClipState shortestEndTrim = TimelineClipEdit::trimEndTo(
        videoOriginal, 0, videoContext);
    require(shortestEndTrim.startFrame == 100
                && shortestEndTrim.durationFrames == 1
                && shortestEndTrim.sourceInFrame == 50,
            "Video end trim must preserve at least one source frame.");
    const TimelineClipState outwardEndTrim = TimelineClipEdit::trimEndTo(
        videoOriginal, 999, videoContext);
    require(outwardEndTrim.startFrame == 100
                && outwardEndTrim.durationFrames == 350,
            "Video end untrim must stop at the source-media duration.");

    const TimelineTrimContext imageContext{ MediaKind::Image, 90 };
    const TimelineClipState imageOriginal{ 100, 200, 0 };
    const TimelineClipState imageStart = TimelineClipEdit::trimStartTo(
        imageOriginal, 40, imageContext);
    require(imageStart.startFrame == 40 && imageStart.durationFrames == 260
                && imageStart.sourceInFrame == 0,
            "Still-image left trim must change when and how long the image appears.");
    const TimelineClipState imageEnd = TimelineClipEdit::trimEndTo(
        imageOriginal, 500, imageContext);
    require(imageEnd.startFrame == 100 && imageEnd.durationFrames == 400
                && imageEnd.sourceInFrame == 0,
            "Still-image right trim must extend display duration without a source limit.");

    TimelineModel timeline;
    const int clipId = timeline.addClip(1, TimelineTrackType::Video, videoOriginal);
    const TimelineGeometry geometry(100, 600);
    const TimelineRectangle rectangle = geometry.clipRectangle(
        *timeline.findClip(clipId));
    const int clipCenterY = rectangle.top + rectangle.height / 2;

    const TimelineClipHit startHit = geometry.hitTestClip(
        timeline.clips(), { rectangle.left + 2, clipCenterY }, clipId, 7);
    require(startHit.region == TimelineClipHitRegion::TrimStart,
            "The selected clip's left edge must be a start-trim handle.");
    const TimelineClipHit endHit = geometry.hitTestClip(
        timeline.clips(), { rectangle.left + rectangle.width - 3, clipCenterY }, clipId, 7);
    require(endHit.region == TimelineClipHitRegion::TrimEnd,
            "The selected clip's right edge must be an end-trim handle.");
    const TimelineClipHit bodyHit = geometry.hitTestClip(
        timeline.clips(), { rectangle.left + rectangle.width / 2, clipCenterY }, clipId, 7);
    require(bodyHit.region == TimelineClipHitRegion::Body,
            "The selected clip's center must remain a move target.");
    const TimelineClipHit unselectedEdgeHit = geometry.hitTestClip(
        timeline.clips(), { rectangle.left + 2, clipCenterY }, 0, 7);
    require(unselectedEdgeHit.region == TimelineClipHitRegion::Body,
            "Trim handles must activate only after the clip is selected.");
}

void mediaLibraryOwnsStableSourceAssetIds()
{
    MediaLibrary library;
    const int builtInId = library.addKnownAsset(L"D:/media/built-in.mp4", MediaKind::Video,
                                                300, 0xB6742D);
    const auto imageId = library.addFile(L"D:/media/cover.jpg");
    const auto audioId = library.addFile(L"D:/media/music.mp3");
    const auto videoId = library.addFile(L"D:/media/clip.mp4");
    require(imageId && audioId && videoId, "Supported files must be importable.");
    require(builtInId == 1 && *imageId == 2,
            "All catalog entries, built-in or imported, must share one stable ID sequence.");
    require(library.findAsset(builtInId)->thumbnailColorRgb == 0xB6742D,
            "A known asset must preserve its supplied presentation color.");
    require(library.findAsset(*imageId)->timelineDurationFrames == 90,
            "Still images must receive a three-second default duration.");
    require(library.findAsset(*audioId)->kind == MediaKind::Audio,
            "Audio extension must infer the audio media kind.");
    require(!library.addFile(L"D:/media/notes.txt"),
            "Unsupported files must be rejected by the media library.");
    require(library.removeAsset(*audioId), "Imported assets must be removable.");
    require(library.findAsset(*videoId) != nullptr,
            "Removing one asset must not invalidate another asset ID.");
}

void workspaceLayoutProtectsPaneBounds()
{
    WorkspaceLayout layout;
    WorkspaceGeometry geometry = layout.calculate(1500, 900);

    require(geometry.mediaLibrary.width >= 340,
            "Media library must remain wide enough for two asset cards.");
    require(geometry.properties.width >= 310,
            "Properties must keep enough width for complete editors.");
    require(geometry.previewCanvas.width >= 320, "Preview must respect its minimum width.");
    require(geometry.timeline.height >= 240,
            "Timeline must remain tall enough to show its complete canvas.");
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
        version5ProjectMigratesToSourceInZero();
        projectSerializerValidatesTimedSourcesButAllowsStillDuration();
        timelineModelStartsEmptyAndOwnsIndependentClipIds();
        editorSessionUndoRedoTracksModelClipMoves();
        editorSessionUndoRedoTracksLibraryInsertion();
        editorSessionUsesRequestedTimelineClipDuration();
        editorSessionUndoRedoTracksClipDeletion();
        focusedTimelineClipOwnsIndependentPlacementSettings();
        playbackStopsAtFocusedPreviewDuration();
        timelineSlotResolutionReturnsNothingForGaps();
        singleTrackPolicyPreventsOverlapAndFindsNearestGap();
        timelineGeometryOwnsFrameworkNeutralCoordinatesAndHitTesting();
        timelineClipTrimmingPreservesValidRangesAndHandleHits();
        mediaLibraryOwnsStableSourceAssetIds();
        workspaceLayoutProtectsPaneBounds();
        std::cout << "MiniEditorCoreTests passed.\n";
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "MiniEditorCoreTests failed: " << exception.what() << '\n';
        return 1;
    }
}
