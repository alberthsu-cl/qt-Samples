#include "EditorSession.h"
#include "WorkspaceLayout.h"

#include <exception>
#include <iostream>
#include <stdexcept>

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
    int notificationCount = 0;
    session.setStateChangedHandler([&notificationCount] { ++notificationCount; });

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
    require(notificationCount == 7, "Every user-visible state change must notify once.");
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
        workspaceLayoutProtectsPaneBounds();
        std::cout << "MiniEditorCoreTests passed.\n";
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "MiniEditorCoreTests failed: " << exception.what() << '\n';
        return 1;
    }
}
