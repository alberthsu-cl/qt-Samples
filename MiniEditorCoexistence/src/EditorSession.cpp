#include "EditorSession.h"

#include <algorithm>
#include <utility>

namespace {

constexpr int kFirstFrame = 0;
constexpr int kLastFrame = 299;
constexpr int kMinimumTimelineZoom = 50;
constexpr int kMaximumTimelineZoom = 200;

} // namespace

EditorSession::EditorSession(std::size_t assetCount)
    : clipSettings_(std::max<std::size_t>(assetCount, 1))
{
}

int EditorSession::selectedAssetIndex() const
{
    return selectedAssetIndex_;
}

const ClipSettings &EditorSession::selectedClipSettings() const
{
    return clipSettings_[selectedAssetIndex_];
}

const PlaybackState &EditorSession::playbackState() const
{
    return playbackState_;
}

const TimelineViewState &EditorSession::timelineViewState() const
{
    return timelineViewState_;
}

void EditorSession::selectAsset(int assetIndex)
{
    selectedAssetIndex_ = std::clamp(assetIndex, 0,
                                     static_cast<int>(clipSettings_.size()) - 1);
    notifyStateChanged();
}

void EditorSession::updateSelectedClipSettings(const ClipSettings &settings)
{
    clipSettings_[selectedAssetIndex_] = settings;
    notifyStateChanged();
}

void EditorSession::handlePlaybackCommand(PlaybackCommand command)
{
    switch (command) {
    case PlaybackCommand::TogglePlayPause:
        playbackState_.isPlaying = !playbackState_.isPlaying;
        break;
    case PlaybackCommand::Stop:
        playbackState_.isPlaying = false;
        playbackState_.currentFrame = kFirstFrame;
        break;
    case PlaybackCommand::StepBackward:
        playbackState_.isPlaying = false;
        playbackState_.currentFrame = std::max(kFirstFrame, playbackState_.currentFrame - 1);
        break;
    case PlaybackCommand::StepForward:
        playbackState_.isPlaying = false;
        playbackState_.currentFrame = std::min(kLastFrame, playbackState_.currentFrame + 1);
        break;
    }

    notifyStateChanged();
}

void EditorSession::advancePlaybackFrame()
{
    if (!playbackState_.isPlaying)
        return;

    playbackState_.currentFrame = (playbackState_.currentFrame + 1) % (kLastFrame + 1);
    notifyStateChanged();
}

void EditorSession::seekTimeline(int frame)
{
    playbackState_.currentFrame = std::clamp(frame, kFirstFrame, kLastFrame);
    notifyStateChanged();
}

void EditorSession::updateTimelineViewState(const TimelineViewState &state)
{
    timelineViewState_ = state;
    timelineViewState_.zoomPercent = std::clamp(timelineViewState_.zoomPercent,
                                                 kMinimumTimelineZoom,
                                                 kMaximumTimelineZoom);
    notifyStateChanged();
}

void EditorSession::fitTimeline()
{
    timelineViewState_.zoomPercent = 100;
    notifyStateChanged();
}

void EditorSession::restoreWorkspaceState(int selectedAssetIndex,
                                          const TimelineViewState &timelineViewState)
{
    selectedAssetIndex_ = std::clamp(selectedAssetIndex, 0,
                                     static_cast<int>(clipSettings_.size()) - 1);
    timelineViewState_ = timelineViewState;
    timelineViewState_.zoomPercent = std::clamp(timelineViewState_.zoomPercent,
                                                 kMinimumTimelineZoom,
                                                 kMaximumTimelineZoom);
}

void EditorSession::setStateChangedHandler(StateChangedHandler handler)
{
    stateChangedHandler_ = std::move(handler);
}

void EditorSession::notifyStateChanged()
{
    if (stateChangedHandler_)
        stateChangedHandler_();
}
