#include "ClipFade.h"
#include "ClipPropertiesStateResolver.h"
#include "EditorSession.h"
#include "EditorCommandController.h"
#include "FrameTimecode.h"
#include "PlaybackClockController.h"
#include "PlaybackBackend.h"
#include "ProjectSerializer.h"
#include "MediaLibrary.h"
#include "MediaPlaybackPlan.h"
#include "TimelineModel.h"
#include "TimelineClipEdit.h"
#include "TimelineTrackPolicy.h"
#include "TimelinePlaybackResolver.h"
#include "TimelinePresentationStateResolver.h"
#include "TimelineGeometry.h"
#include "TimelineEditingController.h"
#include "ThumbnailRequestModel.h"
#include "PreviewStateResolver.h"
#include "ProjectDocumentService.h"
#include "WorkspaceLayout.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void thumbnailRequestModelUsesSourceTimeForTimelineStrips()
{
    TimelineClip videoClip;
    videoClip.mediaAssetId = 7;
    videoClip.state.sourceInFrame = 90;
    videoClip.state.durationFrames = 180;

    const std::vector<ThumbnailRequest> videoRequests =
        ThumbnailRequestModel::timelineStrip(videoClip, MediaKind::Video, 250);
    require(videoRequests.size() == 3,
            "A 250-pixel video clip should request three timeline thumbnails.");
    require(videoRequests.front().sourceFrame == 90
                && videoRequests.back().sourceFrame == 210,
            "Timeline thumbnails must sample the clip's trimmed source range.");

    const std::vector<ThumbnailRequest> imageRequests =
        ThumbnailRequestModel::timelineStrip(videoClip, MediaKind::Image, 250);
    require(imageRequests.size() == 3
                && std::all_of(imageRequests.begin(), imageRequests.end(),
                               [](const ThumbnailRequest &request) {
                                   return request.sourceFrame == 0;
                               }),
            "Still-image timeline thumbnails must always use source frame zero.");
    require(ThumbnailRequestModel::timelineStrip(videoClip, MediaKind::Audio, 250).empty(),
            "Audio clips must not request image thumbnails.");
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

void splittingClipPreservesSourceRangesSettingsAndUndoHistory()
{
    EditorSession session(2);
    const int originalId = session.addTimelineClip(
        1, TimelineTrackType::Video, 100, 200);
    require(originalId > 0, "The video clip used by the split test must be added.");
    require(session.moveTimelineClip(originalId, { 100, 200, 40 }),
            "The split test must establish a non-zero video source-in.");
    session.selectTimelineClip(originalId);
    session.updateSelectedClipSettings({ 75, 125, ClipPosition::TopLeft });
    const int durationBeforeSplit = session.timelineModel().contentDurationFrames();

    require(session.splitTimelineClip(originalId, 100, MediaKind::Video) == 0
                && session.splitTimelineClip(originalId, 300, MediaKind::Video) == 0,
            "A split must be strictly inside the selected clip.");
    const int rightId = session.splitTimelineClip(
        originalId, 150, MediaKind::Video);
    require(rightId > 0, "Splitting a video clip at an interior frame must succeed.");

    const TimelineClip *left = session.timelineModel().findClip(originalId);
    const TimelineClip *right = session.timelineModel().findClip(rightId);
    require(left != nullptr && right != nullptr,
            "A split must retain the original left clip and create a right clip.");
    require(left->state.startFrame == 100 && left->state.durationFrames == 50
                && left->state.sourceInFrame == 40,
            "The left split must retain the original start and source-in.");
    require(right->state.startFrame == 150 && right->state.durationFrames == 150
                && right->state.sourceInFrame == 90,
            "The right video split must advance source-in by the left duration.");
    require(right->mediaAssetId == left->mediaAssetId
                && right->settings.opacityPercent == 75
                && right->settings.scalePercent == 125
                && right->settings.position == ClipPosition::TopLeft,
            "Both split pieces must reference the same asset and placement settings.");
    require(session.timelineModel().contentDurationFrames() == durationBeforeSplit,
            "Splitting must not change total timeline duration or move other clips.");
    require(session.selectedTimelineClipId() == rightId,
            "The new right-hand clip must become the focused placement.");

    require(session.undo(), "A split must undo as one atomic command.");
    left = session.timelineModel().findClip(originalId);
    require(left != nullptr && left->state.durationFrames == 200
                && left->state.sourceInFrame == 40
                && session.timelineModel().findClip(rightId) == nullptr,
            "Undo must restore the original unsplit source range.");
    require(session.selectedTimelineClipId() == originalId,
            "Undo must restore selection to the original clip.");
    require(session.redo(), "A split must redo as one atomic command.");
    require(session.timelineModel().findClip(rightId) != nullptr
                && session.selectedTimelineClipId() == rightId,
            "Redo must restore the same right clip identity and selection.");

    EditorSession imageSession(1);
    const int imageId = imageSession.addTimelineClip(
        1, TimelineTrackType::Video, 0, 90);
    const int imageRightId = imageSession.splitTimelineClip(
        imageId, 30, MediaKind::Image);
    require(imageRightId > 0
                && imageSession.timelineModel().findClip(imageId)
                       ->state.sourceInFrame == 0
                && imageSession.timelineModel().findClip(imageRightId)
                       ->state.sourceInFrame == 0,
            "Still-image splits must keep source-in at zero on both pieces.");
}

void timelineFocusDoesNotRequireASelectedClip()
{
    EditorSession session(2);
    const int clipId = session.addTimelineClip(
        1, TimelineTrackType::Video, 0, 100);
    session.selectTimelineClip(clipId);
    require(session.isTimelineFocused()
                && session.selectedTimelineClipId() == clipId,
            "Selecting a clip must give focus to the timeline workspace.");

    session.focusTimeline();
    require(session.isTimelineFocused()
                && session.selectedTimelineClipId() == 0,
            "The timeline must retain focus when its empty area is selected.");

    session.selectAsset(1);
    require(!session.isTimelineFocused()
                && session.selectedTimelineClipId() == 0,
            "Selecting a library asset must return focus to source preview.");

    session.selectTimelineClip(clipId);
    require(session.removeTimelineClip(clipId),
            "The focused clip must be removable.");
    require(session.isTimelineFocused()
                && session.selectedTimelineClipId() == 0,
            "Deleting the focused clip must leave an unfocused timeline, not select media.");
}

void timelineEditingControllerCoordinatesFocusAndSplitPolicy()
{
    MediaLibrary library;
    const int videoId = library.addKnownAsset(
        L"D:/media/video.mp4", MediaKind::Video, 400, 0x5078A0);
    const int secondVideoId = library.addKnownAsset(
        L"D:/media/second.mp4", MediaKind::Video, 300, 0x7850A0);
    const int audioId = library.addKnownAsset(
        L"D:/media/audio.wav", MediaKind::Audio, 500, 0x2878B4);

    EditorSession session(3);
    TimelineEditingController controller(session, library);
    const int videoClipId = session.addTimelineClip(
        videoId, TimelineTrackType::Video, 100, 100);
    const int audioClipId = session.addTimelineClip(
        audioId, TimelineTrackType::Audio, 90, 200);

    require(controller.insertMediaAsset(secondVideoId, 300),
            "Controller insertion must accept a known library asset.");
    const int insertedClipId = session.selectedTimelineClipId();
    const TimelineClip *insertedClip = session.timelineModel().findClip(insertedClipId);
    require(insertedClip != nullptr
                && insertedClip->mediaAssetId == secondVideoId
                && insertedClip->state.startFrame == 300
                && session.isTimelineFocused()
                && session.selectedAssetIndex() == 1
                && session.timelinePlaybackState().currentFrame == 300,
            "Drag insertion must focus the new placement for Properties editing.");

    require(session.undo(),
            "Drag insertion must undo as one interaction transaction.");
    require(session.timelineModel().findClip(insertedClipId) == nullptr
                && !session.isTimelineFocused()
                && session.selectedTimelineClipId() == 0
                && session.timelinePlaybackState().currentFrame == 0,
            "Undo must restore clips, previous focus, and the previous playhead together.");
    require(session.redo(),
            "Drag insertion must redo as one interaction transaction.");
    require(session.timelineModel().findClip(insertedClipId) != nullptr
                && session.isTimelineFocused()
                && session.selectedTimelineClipId() == insertedClipId
                && session.selectedAssetIndex() == 1
                && session.timelinePlaybackState().currentFrame == 300,
            "Redo must restore the inserted clip, its focus, and its playhead together.");
    const ClipPropertiesViewState insertedProperties =
        ClipPropertiesStateResolver::resolve(session, library);
    require(insertedProperties.target == ClipPropertiesTarget::TimelineClip,
            "Redoing an insertion must restore its editable Properties target.");

    controller.selectSourceAsset(1);
    session.seekTimeline(50);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    require(session.sourcePlaybackState().isPaused
                && session.sourcePlaybackState().currentFrame == 50,
            "The source-selection reset test requires a paused source preview.");
    controller.selectSourceAsset(0);
    require(!session.isTimelineFocused()
                && session.sourcePlaybackState().currentFrame == 0
                && !session.sourcePlaybackState().isPaused,
            "Selecting a different library source must start its preview at frame zero.");

    controller.selectSourceAsset(1);
    session.seekTimeline(50);
    require(controller.focusClip(videoClipId, false)
                && session.isTimelineFocused()
                && session.selectedTimelineClipId() == videoClipId
                && session.selectedAssetIndex() == 0
                && session.playbackState().currentFrame == 50,
            "Controller clip focus must map the placement to its source asset without resetting time.");

    // Reproduce the UI ordering regression: pause over the first placement,
    // then click a different clip without moving the timeline head.
    session.seekTimeline(120);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    require(session.timelinePlaybackState().isPaused,
            "The selection regression requires a paused timeline preview.");
    require(controller.focusClip(insertedClipId, false),
            "A paused timeline must allow selecting another clip for editing.");
    const PreviewState selectedWhilePaused =
        PreviewStateResolver::resolve(session, library);
    require(!session.timelinePlaybackState().isPaused
                && session.timelinePlaybackState().currentFrame == 120
                && selectedWhilePaused.mediaAssetId == secondVideoId
                && selectedWhilePaused.sourceFrame == 0,
            "A clip click must keep the head but preview the selected clip's first frame.");

    controller.focusFrame(120);
    require(session.selectedTimelineClipId() == videoClipId,
            "V1 must win focus when video and audio overlap at the timeline head.");
    controller.focusFrame(220);
    require(session.selectedTimelineClipId() == audioClipId
                && session.selectedAssetIndex() == 2,
            "A1 must receive focus when it is the only placement under the timeline head.");
    controller.focusFrame(50);
    require(session.isTimelineFocused()
                && session.selectedTimelineClipId() == 0
                && session.playbackState().currentFrame == 50,
            "A timeline gap must keep timeline focus while clearing placement focus.");

    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    session.seekTimeline(220);
    controller.followPlaybackFrame();
    require(session.playbackState().isPlaying
                && session.selectedTimelineClipId() == audioClipId
                && session.selectedAssetIndex() == 2,
            "Playback following must highlight its active placement without pausing or resetting it.");

    controller.focusFrame(120);
    require(controller.canCopy() && controller.canCut()
                && controller.canDuplicate() && controller.canSplitAtHead(),
            "Controller command state must follow the placement under the head.");
    require(controller.splitAtHead(),
            "Controller must split a selected placement at an interior head position.");
    const TimelineClip *rightClip = session.timelineModel().findClip(
        session.selectedTimelineClipId());
    require(rightClip != nullptr && rightClip->id != videoClipId
                && rightClip->mediaAssetId == videoId
                && rightClip->state.startFrame == 120
                && session.playbackState().currentFrame == 120,
            "Split must focus the new right placement and preserve the timeline head.");

    require(secondVideoId != videoId,
            "Media library IDs used by controller mapping must remain distinct.");
}

void editorCommandControllerUnifiesEditorIntent()
{
    MediaLibrary library;
    const int videoId = library.addKnownAsset(
        L"D:/media/video.mp4", MediaKind::Video, 300, 0x5078A0);

    EditorSession session(1);
    TimelineEditingController timeline(session, library);
    SimulatedPlaybackBackend playbackBackend(session);
    EditorCommandController commands(session, timeline, playbackBackend);
    const int clipId = session.addTimelineClip(
        videoId, TimelineTrackType::Video, 0, 100);
    session.selectTimelineClip(clipId, 0);
    session.setPlaybackDuration(300, true);

    require(commands.canExecute(EditorIntent::CopyClip)
                && commands.execute(EditorIntent::CopyClip).executed
                && commands.canExecute(EditorIntent::PasteClip),
            "Copy through the command controller must enable Paste through the same policy.");
    session.seekTimeline(150);
    const EditorCommandResult pasted = commands.execute(EditorIntent::PasteClip);
    require(pasted.executed && pasted.playbackTimerNeedsSync
                && session.timelineModel().clips().size() == 2,
            "Timeline insertion commands must report both execution and timer synchronization.");

    session.seekTimeline(200);
    require(commands.canExecute(EditorIntent::SplitClip)
                && commands.execute(EditorIntent::SplitClip).executed,
            "Split must use the shared command policy at the current timeline head.");
    require(commands.canExecute(EditorIntent::Undo)
                && commands.execute(EditorIntent::Undo).executed,
            "Undo availability and execution must be centralized with edit commands.");

    const EditorCommandResult play = commands.execute(EditorIntent::TogglePlayback);
    require(play.executed && play.playbackTimerNeedsSync
                && session.playbackState().isPlaying,
            "Playback commands must report that the native playback timer needs synchronization.");
    const EditorCommandResult stop = commands.execute(EditorIntent::StopPlayback);
    require(stop.executed && stop.playbackTimerNeedsSync
                && !session.playbackState().isPlaying
                && session.playbackState().currentFrame == 0,
            "Stop must be a shared command rather than a UI-specific playback policy.");
}

void playbackClockControllerKeepsTimerPolicyFrameworkNeutral()
{
    EditorSession session(1);
    PlaybackClockController clock(session);
    session.setPlaybackDuration(2, true);

    require(clock.synchronize() == PlaybackClockAction::Stop,
            "A stopped session must tell every UI timer host to stop.");

    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    require(clock.synchronize() == PlaybackClockAction::EnsureRunning,
            "Starting playback must request a running UI timer without depending on MFC or Qt.");
    require(clock.advanceOneFrame() == PlaybackClockAction::EnsureRunning
                && session.playbackState().currentFrame == 1,
            "An interior playback tick must advance the playhead and retain the timer.");
    require(clock.advanceOneFrame() == PlaybackClockAction::Stop
                && !session.playbackState().isPlaying
                && session.playbackState().currentFrame == 1,
            "The final frame must stop the UI timer while preserving the last valid frame.");
}

void previewStateResolverKeepsPreviewPolicyFrameworkNeutral()
{
    MediaLibrary library;
    const int videoId = library.addKnownAsset(
        L"D:/media/video.mp4", MediaKind::Video, 400, 0x5078A0);
    const int audioId = library.addKnownAsset(
        L"D:/media/audio.wav", MediaKind::Audio, 500, 0x2878B4);
    const int imageId = library.addKnownAsset(
        L"D:/media/still.png", MediaKind::Image, 90, 0x7850A0);

    EditorSession session(3);
    PreviewState preview = PreviewStateResolver::resolve(session, library);
    require(preview.mode == PreviewMode::Source && !preview.hasMedia,
            "A fresh editor session must not preview an arbitrary library asset.");

    session.selectAsset(0);
    session.setPlaybackDuration(400, true);
    session.seekTimeline(50);
    preview = PreviewStateResolver::resolve(session, library);
    require(preview.mode == PreviewMode::Source && preview.hasMedia
                && preview.mediaAssetId == videoId && preview.sourceFrame == 50,
            "Source preview must resolve the selected timed asset at its playback frame.");

    session.selectAsset(2);
    session.setPlaybackDuration(90, true);
    session.seekTimeline(20);
    preview = PreviewStateResolver::resolve(session, library);
    require(preview.mediaAssetId == imageId && preview.sourceFrame == 0,
            "A still-image source preview must always use source frame zero.");

    const int videoClipId = session.addTimelineClip(
        videoId, TimelineTrackType::Video, 100, 100);
    const int audioClipId = session.addTimelineClip(
        audioId, TimelineTrackType::Audio, 120, 60);
    require(session.moveTimelineClip(videoClipId, { 100, 100, 20 }),
            "Preview resolver test requires a video clip with a source-in frame.");
    require(session.moveTimelineClip(audioClipId, { 120, 60, 10 }),
            "Preview resolver test requires a timed audio placement.");

    session.selectTimelineClip(videoClipId, 0);
    session.updateSelectedClipSettings({ 70, 125, ClipPosition::BottomRight });
    session.setPlaybackDuration(session.timelineModel().contentDurationFrames(), false);
    session.seekTimeline(50);
    preview = PreviewStateResolver::resolve(session, library);
    const std::optional<ResolvedTimelineMedia> focusedVideo =
        PreviewStateResolver::resolveTimelineVideo(session, library);
    require(preview.mode == PreviewMode::Timeline && preview.hasMedia
                && preview.mediaAssetId == videoId && preview.sourceFrame == 20
                && preview.settings.opacityPercent == 70
                && focusedVideo && focusedVideo->clipId == videoClipId
                && focusedVideo->sourceFrame == 20,
            "A stopped focused video clip and decoder must resolve the same clip start.");

    session.seekTimeline(150);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    preview = PreviewStateResolver::resolve(session, library);
    require(preview.hasMedia && preview.sourceFrame == 70
                && preview.hasAudio && preview.audioSourceFrame == 40,
            "A paused timeline preview must resolve video and audio at the playhead.");

    session.focusTimeline();
    session.setPlaybackDuration(session.timelineModel().contentDurationFrames(), false);
    session.seekTimeline(50);
    preview = PreviewStateResolver::resolve(session, library);
    require(preview.mode == PreviewMode::Timeline && !preview.hasMedia
                && !preview.hasAudio && preview.timelineFrame == 50,
            "An unfocused timeline gap must resolve to an empty preview without fallback media.");
}

void projectDocumentServiceMaintainsProjectAndMediaConsistency()
{
    MediaLibrary library;
    library.addKnownAsset(L"D:/media/default-video.mp4", MediaKind::Video, 300, 0x5078A0);
    library.addKnownAsset(L"D:/media/default-audio.wav", MediaKind::Audio, 300, 0x2878B4);

    EditorSession session(2);
    ProjectDocumentService documents(session, library);
    documents.setDefaultMediaLibrary(library);

    require(!documents.importMedia(L"D:/media/unsupported.txt").succeeded()
                && library.assets().size() == 2,
            "Unsupported import must leave the document media catalog unchanged.");
    require(documents.importMedia(L"D:/media/imported.mp4").succeeded()
                && library.assets().size() == 3
                && session.projectSnapshot().clipSettings.size() == 3,
            "Media import must add matching library and source-setting rows.");

    const int importedAssetId = library.assets().back().id;
    const int clipId = session.addTimelineClip(
        importedAssetId, TimelineTrackType::Video, 0, 120);
    require(documents.removeMedia(2, importedAssetId).error
                == ProjectDocumentError::MediaUsedByTimeline,
            "The document service must protect media still referenced by a timeline clip.");
    require(session.removeTimelineClip(clipId)
                && documents.removeMedia(2, importedAssetId).succeeded()
                && library.assets().size() == 2
                && session.projectSnapshot().clipSettings.size() == 2,
            "Removing an unused asset must keep the library and session source rows aligned.");

    require(documents.importMedia(L"D:/media/saved.mp4").succeeded(),
            "The service save test requires one imported asset.");
    const std::filesystem::path testPath = std::filesystem::temp_directory_path()
        / "mini_editor_document_service_test.mini-editor.json";
    require(documents.save(testPath).succeeded() && !session.isProjectDirty(),
            "Saving through the service must serialize media and clear the dirty state.");

    require(documents.createNewProject().succeeded()
                && library.assets().size() == 2
                && session.projectSnapshot().clipSettings.size() == 2,
            "New Project must restore both the default catalog and default editor state.");
    require(documents.load(testPath).succeeded()
                && library.assets().size() == 3
                && session.projectSnapshot().clipSettings.size() == 3,
            "Loading through the service must restore media and matching editor state.");

    std::error_code removeError;
    std::filesystem::remove(testPath, removeError);
}

void internalTimelineClipboardSupportsCopyCutPasteAndDuplicate()
{
    EditorSession session(2);
    const int originalId = session.addTimelineClip(
        2, TimelineTrackType::Video, 0, 100);
    require(session.moveTimelineClip(originalId, { 0, 100, 25 }),
            "The clipboard test must establish a trimmed source range.");
    session.selectTimelineClip(originalId, 1);
    session.updateSelectedClipSettings(
        { 65, 140, ClipPosition::BottomRight });

    require(session.copySelectedTimelineClip()
                && session.hasTimelineClipboard()
                && session.timelineClipboardMediaAssetId() == 2,
            "Copy must capture the focused placement in the internal clipboard.");
    require(session.timelineModel().clips().size() == 1,
            "Copy must not edit the timeline or create another clip.");

    const int pastedId = session.pasteTimelineClip(200);
    const TimelineClip *pasted = session.timelineModel().findClip(pastedId);
    require(pasted != nullptr && pasted->state.startFrame == 200
                && pasted->state.durationFrames == 100
                && pasted->state.sourceInFrame == 25,
            "Paste must preserve duration/source-in and use the requested free position.");
    require(pasted->mediaAssetId == 2
                && pasted->trackType == TimelineTrackType::Video
                && pasted->settings.opacityPercent == 65
                && pasted->settings.scalePercent == 140
                && pasted->settings.position == ClipPosition::BottomRight,
            "Paste must preserve media, track, and placement properties.");
    require(session.selectedTimelineClipId() == pastedId
                && session.selectedAssetIndex() == 1,
            "Paste must focus the new placement and its source asset.");
    require(session.undo(), "Paste must undo as one timeline command.");
    require(session.timelineModel().findClip(pastedId) == nullptr,
            "Undo must remove the pasted placement.");
    require(session.redo(), "Paste must redo as one timeline command.");
    require(session.timelineModel().findClip(pastedId) != nullptr
                && session.selectedTimelineClipId() == pastedId,
            "Redo must restore the same pasted clip ID and focus.");

    session.selectTimelineClip(originalId, 1);
    const int duplicatedId = session.duplicateSelectedTimelineClip();
    const TimelineClip *duplicated = session.timelineModel().findClip(duplicatedId);
    require(duplicated != nullptr && duplicated->state.startFrame == 100,
            "Duplicate must use the first free position after the focused clip.");

    require(session.cutSelectedTimelineClip(),
            "Cut must copy and remove the focused duplicate.");
    require(session.timelineModel().findClip(duplicatedId) == nullptr,
            "Cut must remove the focused placement.");
    require(session.undo(), "Cut must undo as one timeline command.");
    require(session.timelineModel().findClip(duplicatedId) != nullptr
                && session.selectedTimelineClipId() == duplicatedId,
            "Undo Cut must restore the removed placement and focus.");

    EditorSession rippleSession(2);
    TimelineViewState rippleView;
    rippleView.isRippleEditingEnabled = true;
    rippleSession.updateTimelineViewState(rippleView);
    const int rippleFirstId = rippleSession.addTimelineClip(
        1, TimelineTrackType::Video, 0, 100);
    const int rippleFollowerId = rippleSession.addTimelineClip(
        2, TimelineTrackType::Video, 100, 100);
    rippleSession.selectTimelineClip(rippleFirstId, 0);
    require(rippleSession.copySelectedTimelineClip(),
            "Ripple paste requires a copied source placement.");
    const int ripplePastedId = rippleSession.pasteTimelineClip(100);
    require(rippleSession.timelineModel().findClip(ripplePastedId)
                    ->state.startFrame == 100
                && rippleSession.timelineModel().findClip(rippleFollowerId)
                    ->state.startFrame == 200,
            "Ripple paste must open the copied duration on only its track.");
    require(rippleSession.undo(), "Ripple paste must undo atomically.");
    require(rippleSession.timelineModel().findClip(ripplePastedId) == nullptr
                && rippleSession.timelineModel().findClip(rippleFollowerId)
                    ->state.startFrame == 100,
            "Undo Ripple Paste must restore the complete earlier timeline.");
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
    // Source preview preserves its final frame for inspection.
    EditorSession session(1);
    session.setPlaybackDuration(3, true);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    session.advancePlaybackFrame();
    session.advancePlaybackFrame();
    session.advancePlaybackFrame();
    require(!session.playbackState().isPlaying,
            "Preview playback must stop when its focused duration is reached.");
    require(session.playbackState().isPaused,
            "Natural playback end must preserve the final resolved preview frame.");
    require(session.playbackState().currentFrame == 2,
            "Stopped preview playback must remain on its final valid frame.");

    // Timeline completion instead rewinds and enters a stopped state so the
    // next Play command starts immediately from the beginning.
    const int timelineClipId = session.addTimelineClip(
        1, TimelineTrackType::Video, 0, 3);
    session.selectTimelineClip(timelineClipId, 0);
    session.setPlaybackDuration(3, true);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    session.advancePlaybackFrame();
    session.advancePlaybackFrame();
    session.advancePlaybackFrame();
    require(!session.timelinePlaybackState().isPlaying
                && !session.timelinePlaybackState().isPaused
                && session.timelinePlaybackState().currentFrame == 0,
            "Natural timeline completion must stop and return the head to frame zero.");
}

void pausedPlaybackPreservesItsCurrentFrame()
{
    EditorSession session(1);
    session.setPlaybackDuration(20, true);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    session.advancePlaybackFrame();
    session.advancePlaybackFrame();
    const int pausedFrame = session.playbackState().currentFrame;

    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    require(!session.playbackState().isPlaying && session.playbackState().isPaused,
            "Play/Pause must enter an explicit paused state.");
    require(session.playbackState().currentFrame == pausedFrame,
            "Pausing must preserve the exact current frame.");

    session.handlePlaybackCommand(PlaybackCommand::Stop);
    require(!session.playbackState().isPaused
                && session.playbackState().currentFrame == 0,
            "Stop must remain distinct from Pause and return to frame zero.");
}

void timelinePlaybackResolvesTrimmedSourcesGapsAndStillImages()
{
    MediaLibrary library;
    const int videoId = library.addKnownAsset(
        L"D:/media/video.mp4", MediaKind::Video, 400, 0x5078A0);
    const int audioId = library.addKnownAsset(
        L"D:/media/audio.wav", MediaKind::Audio, 500, 0x2878B4);
    const int imageId = library.addKnownAsset(
        L"D:/media/still.png", MediaKind::Image, 90, 0x7850A0);

    TimelineModel timeline;
    timeline.addClip(videoId, TimelineTrackType::Video, { 100, 100, 50 });
    timeline.addClip(audioId, TimelineTrackType::Audio, { 90, 200, 20 });
    timeline.addClip(imageId, TimelineTrackType::Video, { 250, 90, 0 });

    ResolvedTimelineFrame frame = TimelinePlaybackResolver::resolve(
        timeline, library, 100);
    require(frame.video && frame.video->clipLocalFrame == 0
                && frame.video->sourceFrame == 50,
            "A trimmed video must begin playback at its stored source-in frame.");
    require(frame.audio && frame.audio->clipLocalFrame == 10
                && frame.audio->sourceFrame == 30,
            "V1 and A1 must resolve independently at the same timeline frame.");

    frame = TimelinePlaybackResolver::resolve(timeline, library, 199);
    require(frame.video && frame.video->sourceFrame == 149,
            "The last video placement frame must map to the last included source frame.");
    frame = TimelinePlaybackResolver::resolve(timeline, library, 200);
    require(!frame.video && frame.audio,
            "A timeline gap must return no video while audio may continue.");

    frame = TimelinePlaybackResolver::resolve(timeline, library, 339);
    require(frame.video && frame.video->mediaKind == MediaKind::Image
                && frame.video->clipLocalFrame == 89
                && frame.video->sourceFrame == 0,
            "A still image must advance display time while remaining on source frame zero.");
    frame = TimelinePlaybackResolver::resolve(timeline, library, 340);
    require(!frame.video,
            "A clip's timeline end must be exclusive so the following gap is visible.");
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
    require(TimelineTrackPolicy::magneticallySnappedStart(
                timeline.clips(), TimelineTrackType::Video,
                183, 40, 0, 8) == 180,
            "A nearby drop must magnetically align its left edge to a clip boundary.");
    require(TimelineTrackPolicy::magneticallySnappedStart(
                timeline.clips(), TimelineTrackType::Video,
                191, 40, 0, 8) == 191,
            "A placement outside the magnetic tolerance must remain freely positioned.");

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
    const TimelineClipState magneticEndTrim = TimelineTrackPolicy::constrainEndTrim(
        timeline.clips(), *firstClip, { 0, 235, 0 }, 400, 8);
    require(magneticEndTrim.durationFrames == 240,
            "A trim edge near its neighbor must magnetically close the gap.");

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

void rippleEditingShiftsOneTrackAsOneUndoableCommand()
{
    EditorSession session(3);
    TimelineViewState viewState;
    viewState.isRippleEditingEnabled = true;
    session.updateTimelineViewState(viewState);

    const int firstId = session.addTimelineClip(
        1, TimelineTrackType::Video, 0, 100);
    const int secondId = session.addTimelineClip(
        2, TimelineTrackType::Video, 100, 100);
    const int audioId = session.addTimelineClip(
        3, TimelineTrackType::Audio, 100, 100);
    const int insertedId = session.addTimelineClip(
        3, TimelineTrackType::Video, 100, 30);

    require(session.timelineModel().findClip(firstId)->state.startFrame == 0
                && session.timelineModel().findClip(insertedId)->state.startFrame == 100
                && session.timelineModel().findClip(secondId)->state.startFrame == 130,
            "Ripple insertion must open space on V1 at the requested edit boundary.");
    require(session.timelineModel().findClip(audioId)->state.startFrame == 100,
            "Ripple insertion on V1 must not move clips on A1.");
    require(session.undo(), "Ripple insertion must be one undoable command.");
    require(session.timelineModel().findClip(insertedId) == nullptr
                && session.timelineModel().findClip(secondId)->state.startFrame == 100,
            "Undo must remove the insertion and restore all shifted V1 clips.");
    require(session.redo(), "Ripple insertion must be redoable as one command.");

    require(session.moveTimelineClip(firstId, { 0, 70, 0 },
                                     TimelineClipEditKind::TrimEnd),
            "A ripple end trim must succeed.");
    require(session.timelineModel().findClip(insertedId)->state.startFrame == 70
                && session.timelineModel().findClip(secondId)->state.startFrame == 100,
            "Shortening a clip must pull every following V1 clip left.");
    require(session.timelineModel().findClip(audioId)->state.startFrame == 100,
            "A V1 trim must leave A1 timing unchanged.");
    require(session.undo(), "The multi-clip ripple trim must undo atomically.");
    require(session.timelineModel().findClip(firstId)->state.durationFrames == 100
                && session.timelineModel().findClip(insertedId)->state.startFrame == 100
                && session.timelineModel().findClip(secondId)->state.startFrame == 130,
            "Undo must restore the complete pre-trim timeline snapshot.");

    require(session.moveTimelineClip(firstId, { 0, 70, 0 },
                                     TimelineClipEditKind::TrimEnd),
            "A clip must be shorten-able before testing ripple restoration.");
    require(session.moveTimelineClip(firstId, { 0, 100, 0 },
                                     TimelineClipEditKind::TrimEnd),
            "A shortened ripple trim must extend back into its source media.");
    require(session.timelineModel().findClip(firstId)->state.durationFrames == 100
                && session.timelineModel().findClip(insertedId)->state.startFrame == 100
                && session.timelineModel().findClip(secondId)->state.startFrame == 130,
            "Extending back must push every following clip to its restored position.");
    require(session.undo(), "Ripple re-extension must undo atomically.");
    require(session.timelineModel().findClip(firstId)->state.durationFrames == 70
                && session.timelineModel().findClip(insertedId)->state.startFrame == 70,
            "Undo must restore the shortened ripple state.");
    require(session.undo(), "The setup shortening must also remain undoable.");

    require(session.removeTimelineClip(insertedId),
            "Ripple delete must remove the selected time range.");
    require(session.timelineModel().findClip(insertedId) == nullptr
                && session.timelineModel().findClip(secondId)->state.startFrame == 100,
            "Ripple delete must close time for following clips on the same track.");
    require(session.timelineModel().findClip(audioId)->state.startFrame == 100,
            "Ripple delete on V1 must leave A1 timing unchanged.");
    require(session.undo(), "Ripple delete must undo as one timeline snapshot.");
    require(session.timelineModel().findClip(insertedId) != nullptr
                && session.timelineModel().findClip(secondId)->state.startFrame == 130,
            "Undo must restore the deleted clip and every shifted follower.");

    const int laterId = session.addTimelineClip(
        2, TimelineTrackType::Video, 230, 50);
    require(session.moveTimelineClip(secondId, { 150, 80, 20 },
                                     TimelineClipEditKind::TrimStart),
            "A ripple start trim must succeed.");
    require(session.timelineModel().findClip(secondId)->state.startFrame == 130
                && session.timelineModel().findClip(secondId)->state.durationFrames == 80
                && session.timelineModel().findClip(secondId)->state.sourceInFrame == 20
                && session.timelineModel().findClip(laterId)->state.startFrame == 210,
            "A ripple start trim must preserve the edit point and pull followers left.");
    require(session.undo(), "A ripple start trim must undo atomically.");
    require(session.timelineModel().findClip(secondId)->state.durationFrames == 100
                && session.timelineModel().findClip(laterId)->state.startFrame == 230,
            "Undo must restore the pre-start-trim timeline.");

    require(TimelineTrackPolicy::rippleInsertionStart(
                session.timelineModel().clips(), TimelineTrackType::Video,
                50, 0) == 0,
            "A ripple drop inside a clip must choose the nearer edit boundary.");

    EditorSession moveSession(4);
    moveSession.updateTimelineViewState(viewState);
    const int moveFirstId = moveSession.addTimelineClip(
        1, TimelineTrackType::Video, 0, 100);
    const int moveSecondId = moveSession.addTimelineClip(
        2, TimelineTrackType::Video, 100, 100);
    const int moveThirdId = moveSession.addTimelineClip(
        3, TimelineTrackType::Video, 200, 100);
    const int moveAudioId = moveSession.addTimelineClip(
        4, TimelineTrackType::Audio, 100, 100);

    require(moveSession.moveTimelineClip(
                moveThirdId, { 0, 100, 0 }, TimelineClipEditKind::Move),
            "A whole clip must ripple-move to the left.");
    require(moveSession.timelineModel().findClip(moveThirdId)->state.startFrame == 0
                && moveSession.timelineModel().findClip(moveFirstId)->state.startFrame == 100
                && moveSession.timelineModel().findClip(moveSecondId)->state.startFrame == 200,
            "A left ripple move must open the destination and shift earlier clips right.");
    require(moveSession.timelineModel().findClip(moveAudioId)->state.startFrame == 100,
            "A V1 ripple move must not change A1.");
    require(moveSession.undo(), "A left ripple move must undo atomically.");
    require(moveSession.timelineModel().findClip(moveFirstId)->state.startFrame == 0
                && moveSession.timelineModel().findClip(moveSecondId)->state.startFrame == 100
                && moveSession.timelineModel().findClip(moveThirdId)->state.startFrame == 200,
            "Undo must restore every clip affected by a left ripple move.");

    require(moveSession.moveTimelineClip(
                moveFirstId, { 200, 100, 0 }, TimelineClipEditKind::Move),
            "A whole clip must ripple-move to the right.");
    require(moveSession.timelineModel().findClip(moveSecondId)->state.startFrame == 0
                && moveSession.timelineModel().findClip(moveThirdId)->state.startFrame == 100
                && moveSession.timelineModel().findClip(moveFirstId)->state.startFrame == 200,
            "A right ripple move must close the old gap and open the destination.");
    require(moveSession.undo(), "A right ripple move must undo atomically.");
    require(moveSession.timelineModel().findClip(moveFirstId)->state.startFrame == 0
                && moveSession.timelineModel().findClip(moveSecondId)->state.startFrame == 100
                && moveSession.timelineModel().findClip(moveThirdId)->state.startFrame == 200,
            "Undo must restore every clip affected by a right ripple move.");

    require(TimelineTrackPolicy::rippleMoveStart(
                moveSession.timelineModel().clips(), moveThirdId, 0, 0) == 0,
            "Dragging a later clip left must use the visible left edit boundary.");
    require(TimelineTrackPolicy::rippleMoveStart(
                moveSession.timelineModel().clips(), moveFirstId, 300, 0) == 200,
            "Dragging a clip right must account for closing its original gap.");
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
    const TimelineClipState rippleRestoredStart =
        TimelineClipEdit::rippleTrimStartTo({ 0, 170, 30 }, -30,
                                            videoContext);
    require(rippleRestoredStart.startFrame == -30
                && rippleRestoredStart.durationFrames == 200
                && rippleRestoredStart.sourceInFrame == 0,
            "Ripple start trim must restore source frames even at timeline frame zero.");

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
    require(library.updateAssetDuration(*videoId, 375)
                && library.findAsset(*videoId)->timelineDurationFrames == 375,
            "Decoded media duration must replace an imported asset's provisional duration.");
    require(!library.updateAssetDuration(*videoId, 375),
            "Publishing the same decoded duration must not report another metadata change.");
    require(frameTimecodeMmSsFf(4170) == L"02:19:00"
                && frameTimecodeMmSsFf(375) == L"00:12:15",
            "Properties duration must use readable minute:second:frame timecode.");
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


void clipFadeOwnsRampPolicyIndependentlyOfAnyRenderer()
{
    ClipSettings settings;
    settings.opacityPercent = 100;
    settings.fadeInFrames = 10;
    settings.fadeOutFrames = 20;

    const ClipFadeRange range = ClipFade::normalize(settings, 100);
    require(range.fadeInFrames == 10 && range.fadeOutFrames == 20,
            "Fades that fit inside the clip must be used unchanged.");

    require(ClipFade::gainPercentAt(settings, 0, 100) == 0,
            "A fade-in must start fully transparent.");
    require(ClipFade::gainPercentAt(settings, 5, 100) == 50,
            "A fade-in must ramp linearly to full opacity.");
    require(ClipFade::gainPercentAt(settings, 10, 100) == 100,
            "A fade-in must reach full opacity when its ramp ends.");
    require(ClipFade::gainPercentAt(settings, 50, 100) == 100,
            "Frames between the two ramps must stay at full opacity.");
    require(ClipFade::gainPercentAt(settings, 80, 100) == 100,
            "A fade-out must not begin before its own ramp.");
    require(ClipFade::gainPercentAt(settings, 90, 100) == 50,
            "A fade-out must ramp down linearly.");
    require(ClipFade::gainPercentAt(settings, 99, 100) == 5,
            "A fade-out must approach transparency at the last clip frame.");

    settings.opacityPercent = 50;
    require(ClipFade::effectiveOpacityPercent(settings, 5, 100) == 25,
            "The ramp must scale the stored opacity rather than replace it.");
    require(ClipFade::effectiveOpacityPercent(settings, 50, 100) == 50,
            "A held frame must render exactly the stored opacity.");

    // A trim can leave a clip shorter than its two ramps combined. Rendering
    // must stay valid without rewriting the stored edit decision.
    settings.fadeInFrames = 30;
    settings.fadeOutFrames = 10;
    const ClipFadeRange trimmed = ClipFade::normalize(settings, 20);
    require(trimmed.fadeInFrames == 15 && trimmed.fadeOutFrames == 5,
            "Overlapping ramps must keep their requested proportions inside the clip.");
    require(ClipFade::gainPercentAt(settings, 0, 20) == 0
                && ClipFade::gainPercentAt(settings, 14, 20) == 93,
            "A normalized ramp pair must still produce a valid gain curve.");

    const ClipSettings clamped = ClipFade::clampSettings(settings, 20);
    require(clamped.fadeInFrames == 15 && clamped.fadeOutFrames == 5,
            "Clamping must store fade lengths that already fit the clip.");
    require(ClipFade::gainPercentAt({}, 0, 100) == 100
                && !ClipFade::hasFade({}),
            "A clip without fades must render at full gain everywhere.");
}

void clipFadesTravelThroughSessionUndoAndProjectFiles()
{
    MediaLibrary library;
    const int videoId = library.addKnownAsset(
        L"D:/media/video.mp4", MediaKind::Video, 400, 0x5078A0);
    const int audioId = library.addKnownAsset(
        L"D:/media/audio.wav", MediaKind::Audio, 400, 0x2878B4);

    EditorSession session(2);
    const int videoClipId = session.addTimelineClip(
        videoId, TimelineTrackType::Video, 0, 100);
    const int audioClipId = session.addTimelineClip(
        audioId, TimelineTrackType::Audio, 0, 100);
    session.selectTimelineClip(videoClipId, 0);

    ClipSettings settings;
    settings.fadeInFrames = 40;
    settings.fadeOutFrames = 20;
    settings.effect = ClipEffectKind::Grayscale;
    settings.effectIntensityPercent = 65;
    session.updateSelectedClipSettings(settings);
    const TimelineClip *videoClip = session.timelineModel().findClip(videoClipId);
    require(videoClip != nullptr && videoClip->settings.fadeInFrames == 40
                && videoClip->settings.fadeOutFrames == 20
                && videoClip->settings.effect == ClipEffectKind::Grayscale
                && videoClip->settings.effectIntensityPercent == 65,
            "The session must store fades and DSP on the focused clip.");

    require(session.undo(), "A fade edit must be undoable.");
    require(session.timelineModel().findClip(videoClipId)->settings.fadeInFrames == 0,
            "Undo must restore the previous fade lengths.");
    require(session.timelineModel().findClip(videoClipId)->settings.effect
                == ClipEffectKind::None,
            "Undo must restore the previous DSP effect.");
    require(session.redo(), "A fade edit must be redoable.");
    require(session.timelineModel().findClip(videoClipId)->settings.fadeInFrames == 40,
            "Redo must reapply the stored fade lengths.");
    require(session.timelineModel().findClip(videoClipId)->settings.effect
                == ClipEffectKind::Grayscale,
            "Redo must reapply the stored DSP effect.");

    // Splitting divides the clip, so each half keeps only the ramp it owns.
    const int rightPieceId = session.splitTimelineClip(videoClipId, 60,
                                                       MediaKind::Video);
    require(rightPieceId != 0, "The fade test requires a valid split.");
    const TimelineClip *leftPiece = session.timelineModel().findClip(videoClipId);
    const TimelineClip *rightPiece = session.timelineModel().findClip(rightPieceId);
    require(leftPiece->settings.fadeInFrames == 40
                && leftPiece->settings.fadeOutFrames == 0,
            "The left split piece must keep the fade-in and lose the fade-out.");
    require(rightPiece->settings.fadeInFrames == 0
                && rightPiece->settings.fadeOutFrames == 20,
            "The right split piece must keep the fade-out and lose the fade-in.");
    require(session.undo(), "A split must remain undoable with fades present.");
    require(session.timelineModel().findClip(videoClipId)->settings.fadeOutFrames == 20,
            "Undoing a split must restore both original fade lengths.");

    session.selectTimelineClip(audioClipId, 1);
    ClipSettings audioSettings;
    audioSettings.fadeOutFrames = 50;
    session.updateSelectedClipSettings(audioSettings);

    session.focusTimeline();
    session.setPlaybackDuration(session.timelineModel().contentDurationFrames(), false);
    session.seekTimeline(20);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    PreviewState preview = PreviewStateResolver::resolve(session, library);
    require(preview.videoFadeGainPercent == 50 && preview.effectiveOpacityPercent == 50,
            "A paused playhead inside a fade-in must report the ramped opacity.");
    require(preview.hasAudio && preview.audioFadeGainPercent == 100,
            "An audio clip must keep full level before its fade-out begins.");

    session.seekTimeline(75);
    preview = PreviewStateResolver::resolve(session, library);
    require(preview.audioFadeGainPercent == 50,
            "An audio fade-out must ramp the reported level down.");
    require(preview.videoFadeGainPercent == 100,
            "A frame between two ramps must render at full opacity.");

    // Stopped editing shows the focused clip as an edit target, so its own
    // fade-in must not hide the placement being adjusted.
    session.selectTimelineClip(videoClipId, 0);
    session.handlePlaybackCommand(PlaybackCommand::Stop);
    preview = PreviewStateResolver::resolve(session, library);
    require(preview.videoFadeGainPercent == 100
                && preview.effectiveOpacityPercent == preview.settings.opacityPercent,
            "A stopped focused clip must preview at its stored opacity.");

    EditorProject project = session.projectSnapshot();
    project.mediaAssets = library.assets();
    const std::filesystem::path path = std::filesystem::temp_directory_path()
        / "MiniEditorFadeTests.mini-editor.json";
    std::wstring errorMessage;
    require(ProjectSerializer::save(path, project, &errorMessage),
            "A project containing fades must save.");
    const std::optional<EditorProject> loaded =
        ProjectSerializer::load(path, 2, &errorMessage);
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
    require(loaded.has_value(), "A project containing fades must load again.");
    const auto savedVideoClip = std::find_if(loaded->timelineItems.begin(),
        loaded->timelineItems.end(),
        [videoId](const TimelineClip &clip) { return clip.mediaAssetId == videoId; });
    require(savedVideoClip != loaded->timelineItems.end()
                && savedVideoClip->settings.fadeInFrames == 40
                && savedVideoClip->settings.fadeOutFrames == 20
                && savedVideoClip->settings.effect == ClipEffectKind::Grayscale
                && savedVideoClip->settings.effectIntensityPercent == 65,
            "Format version 8 must round-trip fades and DSP settings.");

    // Save must reject values above the editor's supported maximum even when
    // the clip itself is long enough. Otherwise load would silently clamp the
    // value and the saved project would not round-trip exactly.
    project.timelineItems.front().state.durationFrames = 400;
    project.timelineItems.front().settings.fadeInFrames =
        ClipFade::kMaximumFadeFrames + 1;
    project.timelineItems.front().settings.fadeOutFrames = 0;
    require(!ProjectSerializer::save(path, project, &errorMessage),
            "Saving a fade above the supported maximum must be rejected.");
}

void simulatedPlaybackBackendOwnsTheInitialPlaybackImplementation()
{
    EditorSession session(1);
    SimulatedPlaybackBackend backend(session);
    session.setPlaybackDuration(2, true);

    require(backend.tickIntervalMilliseconds()
                == PlaybackClockController::kTickIntervalMilliseconds,
            "The simulated backend must publish the existing timer cadence to its UI host.");
    require(backend.executeCommand(PlaybackCommand::TogglePlayPause)
                == PlaybackClockAction::EnsureRunning
                && session.playbackState().isPlaying,
            "Transport commands must pass through the backend before changing editor playback.");
    require(backend.advanceOneFrame() == PlaybackClockAction::EnsureRunning
                && session.playbackState().currentFrame == 1,
            "The simulated backend must preserve the existing interior-frame behavior.");
    require(backend.advanceOneFrame() == PlaybackClockAction::Stop
                && !session.playbackState().isPlaying,
            "The backend must stop its host clock when the simulated media reaches its end.");
}

void mediaPlaybackPlanResolverCentralizesDecoderIntent()
{
    MediaLibrary library;
    const int videoId = library.addKnownAsset(
        L"D:/media/video.mp4", MediaKind::Video, 300, 0x5078A0);
    const int imageId = library.addKnownAsset(
        L"D:/media/still.jpg", MediaKind::Image, 150, 0x2878B4);

    EditorSession session(2);
    MediaPlaybackPlan plan = MediaPlaybackPlanResolver::resolve(session, library);
    require(!plan.hasMedia(),
            "An editor with no selection must not request decoder media.");

    session.selectAsset(0);
    session.seekTimeline(45);
    plan = MediaPlaybackPlanResolver::resolve(session, library);
    require(plan.context == MediaPlaybackContext::Source
                && plan.mediaAssetId == videoId
                && plan.timelineClipId == 0
                && plan.sourceFrame == 45
                && plan.usesMediaDecoder()
                && plan.needsSilentVideoPreroll(),
            "A stopped source video must request its selected frame as a silent preroll.");

    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    plan = MediaPlaybackPlanResolver::resolve(session, library);
    require(plan.shouldPlay && !plan.needsSilentVideoPreroll(),
            "A playing source must publish play intent instead of preroll intent.");
    session.handlePlaybackCommand(PlaybackCommand::Stop);

    session.selectAsset(1);
    plan = MediaPlaybackPlanResolver::resolve(session, library);
    require(plan.mediaAssetId == imageId && !plan.usesMediaDecoder()
                && plan.sourceFrame == 0,
            "A still image must remain in the plan without using QMediaPlayer.");

    const int clipId = session.addTimelineClip(
        videoId, TimelineTrackType::Video, 100, 90);
    TimelineClipState trimmed = session.timelineModel().findClip(clipId)->state;
    trimmed.sourceInFrame = 60;
    session.moveTimelineClip(clipId, trimmed, TimelineClipEditKind::TrimStart);
    session.selectTimelineClip(clipId, 0);
    session.seekTimeline(140);

    // Stopped editing previews the selected clip's first trimmed source frame,
    // independently of the timeline head.
    plan = MediaPlaybackPlanResolver::resolve(session, library);
    require(plan.context == MediaPlaybackContext::Timeline
                && plan.timelineClipId == clipId
                && plan.sourceFrame == 60
                && plan.needsSilentVideoPreroll(),
            "A stopped timeline edit target must resolve to its trimmed source-in frame.");

    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    plan = MediaPlaybackPlanResolver::resolve(session, library);
    require(plan.sourceFrame == 100 && plan.shouldPlay,
            "Timeline playback must map the playhead to clip-local trimmed source time.");

    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    plan = MediaPlaybackPlanResolver::resolve(session, library);
    require(plan.sourceFrame == 100 && plan.isPaused && !plan.shouldPlay,
            "A paused timeline must preserve the exact playhead-derived source frame.");
}

void editorSessionAcceptsAuthoritativeBackendPlaybackState()
{
    EditorSession session(1);
    session.updatePlaybackFromBackend(75, 240, true, false);
    require(session.playbackState().currentFrame == 75
                && session.playbackState().durationFrames == 240
                && session.playbackState().isPlaying
                && !session.playbackState().isPaused,
            "A real media backend must be able to publish its clock into the active preview state.");

    session.updatePlaybackFromBackend(500, 240, false, true);
    require(session.playbackState().currentFrame == 239
                && !session.playbackState().isPlaying
                && session.playbackState().isPaused,
            "Backend playback reports must clamp to the decoded duration and preserve pause state.");
}

void clipPropertiesResolverBuildsOneCompleteViewSnapshot()
{
    MediaLibrary library;
    const int videoId = library.addKnownAsset(
        L"D:/media/video.mp4", MediaKind::Video, 240, 0x5078A0);
    library.addKnownAsset(
        L"D:/media/audio.wav", MediaKind::Audio, 180, 0x2878B4);

    EditorSession session(2);
    const int clipId = session.addTimelineClip(
        videoId, TimelineTrackType::Video, 30, 120);
    session.selectTimelineClip(clipId, 0);
    ClipSettings settings;
    settings.opacityPercent = 75;
    settings.scalePercent = 130;
    settings.position = ClipPosition::BottomRight;
    settings.fadeInFrames = 15;
    settings.fadeOutFrames = 25;
    session.updateSelectedClipSettings(settings);

    const ClipPropertiesViewState selected =
        ClipPropertiesStateResolver::resolve(session, library);
    require(selected.editingEnabled
                && selected.target == ClipPropertiesTarget::TimelineClip
                && selected.mediaKind == MediaKind::Video
                && selected.durationFrames == 120
                && selected.settings.opacityPercent == 75
                && selected.settings.fadeInFrames == 15
                && selected.settings.fadeOutFrames == 25,
            "The Properties snapshot must describe the complete focused placement.");

    session.focusTimeline();
    const ClipPropertiesViewState emptyTimelineSelection =
        ClipPropertiesStateResolver::resolve(session, library);
    require(!emptyTimelineSelection.editingEnabled
                && emptyTimelineSelection.target == ClipPropertiesTarget::EmptyTimeline
                && emptyTimelineSelection.durationFrames == 0,
            "Properties must disable editing when the timeline has no focused clip.");

    session.selectAsset(1);
    const ClipPropertiesViewState sourceAsset =
        ClipPropertiesStateResolver::resolve(session, library);
    require(sourceAsset.target == ClipPropertiesTarget::MediaAsset
                && !sourceAsset.editingEnabled
                && sourceAsset.mediaDisplayName == L"audio.wav"
                && sourceAsset.mediaFilePath == L"D:/media/audio.wav"
                && sourceAsset.mediaKind == MediaKind::Audio
                && sourceAsset.durationFrames == 180,
            "A library selection must expose read-only source media information.");
}

void timelinePresentationResolverBuildsOneCompleteViewSnapshot()
{
    EditorSession session(1);
    const int clipId = session.addTimelineClip(
        17, TimelineTrackType::Video, 45, 90);
    session.selectTimelineClip(clipId, 0);
    session.seekTimeline(60);
    TimelineViewState view;
    view.zoomPercent = 150;
    view.isAudioTrackVisible = false;
    view.isRippleEditingEnabled = true;
    session.updateTimelineViewState(view);

    const TimelinePresentationState state =
        TimelinePresentationStateResolver::resolve(session, true);
    require(state.clips.size() == 1
                && state.clips.front().id == clipId
                && state.selectedClipId == clipId
                && state.durationFrames == 600
                && state.playback.currentFrame == 60
                && state.view.zoomPercent == 150
                && !state.view.isAudioTrackVisible
                && state.view.isRippleEditingEnabled
                && state.splitEnabled,
            "The timeline snapshot must contain canvas and toolbar state together.");
}

void sourcePlaybackDoesNotMoveTheTimelinePlayhead()
{
    EditorSession session(1);
    const int clipId = session.addTimelineClip(
        1, TimelineTrackType::Video, 0, 180);
    session.selectTimelineClip(clipId, 0);
    session.setPlaybackDuration(180, true);
    session.seekTimeline(75);

    TimelinePresentationState timelineState =
        TimelinePresentationStateResolver::resolve(session, true);
    require(timelineState.playback.currentFrame == 75,
            "The timeline snapshot must initially follow its own playhead.");

    session.selectAsset(0);
    session.setPlaybackDuration(120, true);
    session.handlePlaybackCommand(PlaybackCommand::TogglePlayPause);
    session.advancePlaybackFrame();
    session.advancePlaybackFrame();
    require(session.playbackState().currentFrame == 2
                && session.sourcePlaybackState().currentFrame == 2,
            "The active transport must advance the focused source preview.");

    timelineState = TimelinePresentationStateResolver::resolve(session, false);
    require(timelineState.playback.currentFrame == 75
                && !timelineState.playback.isPlaying,
            "Source playback must leave the stored timeline playhead unmoved.");

    session.selectTimelineClip(clipId, 0);
    session.setPlaybackDuration(180, false);
    require(session.playbackState().currentFrame == 75
                && session.timelinePlaybackState().currentFrame == 75
                && session.sourcePlaybackState().currentFrame == 2,
            "Returning to timeline focus must restore its independent transport state.");
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
        splittingClipPreservesSourceRangesSettingsAndUndoHistory();
        timelineFocusDoesNotRequireASelectedClip();
        timelineEditingControllerCoordinatesFocusAndSplitPolicy();
        editorCommandControllerUnifiesEditorIntent();
        playbackClockControllerKeepsTimerPolicyFrameworkNeutral();
        simulatedPlaybackBackendOwnsTheInitialPlaybackImplementation();
        mediaPlaybackPlanResolverCentralizesDecoderIntent();
        editorSessionAcceptsAuthoritativeBackendPlaybackState();
        previewStateResolverKeepsPreviewPolicyFrameworkNeutral();
        clipFadeOwnsRampPolicyIndependentlyOfAnyRenderer();
        clipFadesTravelThroughSessionUndoAndProjectFiles();
        clipPropertiesResolverBuildsOneCompleteViewSnapshot();
        timelinePresentationResolverBuildsOneCompleteViewSnapshot();
        sourcePlaybackDoesNotMoveTheTimelinePlayhead();
        projectDocumentServiceMaintainsProjectAndMediaConsistency();
        internalTimelineClipboardSupportsCopyCutPasteAndDuplicate();
        focusedTimelineClipOwnsIndependentPlacementSettings();
        playbackStopsAtFocusedPreviewDuration();
        pausedPlaybackPreservesItsCurrentFrame();
        timelinePlaybackResolvesTrimmedSourcesGapsAndStillImages();
        singleTrackPolicyPreventsOverlapAndFindsNearestGap();
        rippleEditingShiftsOneTrackAsOneUndoableCommand();
        timelineGeometryOwnsFrameworkNeutralCoordinatesAndHitTesting();
        timelineClipTrimmingPreservesValidRangesAndHandleHits();
        thumbnailRequestModelUsesSourceTimeForTimelineStrips();
        mediaLibraryOwnsStableSourceAssetIds();
        workspaceLayoutProtectsPaneBounds();
        std::cout << "MiniEditorCoreTests passed.\n";
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "MiniEditorCoreTests failed: " << exception.what() << '\n';
        return 1;
    }
}
