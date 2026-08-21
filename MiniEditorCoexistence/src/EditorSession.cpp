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
    notifyStateChanged(EditorChange::Selection);
}

void EditorSession::updateSelectedClipSettings(const ClipSettings &settings)
{
    clipSettings_[selectedAssetIndex_] = settings;
    notifyStateChanged(EditorChange::ClipSettings);
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

    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::advancePlaybackFrame()
{
    if (!playbackState_.isPlaying)
        return;

    playbackState_.currentFrame = (playbackState_.currentFrame + 1) % (kLastFrame + 1);
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::seekTimeline(int frame)
{
    playbackState_.currentFrame = std::clamp(frame, kFirstFrame, kLastFrame);
    notifyStateChanged(EditorChange::Playback);
}

void EditorSession::updateTimelineViewState(const TimelineViewState &state)
{
    timelineViewState_ = state;
    timelineViewState_.zoomPercent = std::clamp(timelineViewState_.zoomPercent,
                                                 kMinimumTimelineZoom,
                                                 kMaximumTimelineZoom);
    notifyStateChanged(EditorChange::TimelineView);
}

void EditorSession::fitTimeline()
{
    timelineViewState_.zoomPercent = 100;
    notifyStateChanged(EditorChange::TimelineView);
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

EditorSession::ObserverId EditorSession::addObserver(StateChangedHandler handler)
{
    const ObserverId observerId = nextObserverId_++;
    observers_.push_back({ observerId, std::move(handler) });
    return observerId;
}

void EditorSession::removeObserver(ObserverId observerId)
{
    observers_.erase(std::remove_if(observers_.begin(), observers_.end(),
        [observerId](const Observer &observer) { return observer.id == observerId; }),
        observers_.end());
}

void EditorSession::notifyStateChanged(EditorChange changes)
{
    // Copy first: an observer may safely subscribe or unsubscribe while a
    // notification is being delivered without invalidating this iteration.
    const std::vector<Observer> observers = observers_;
    for (const Observer &observer : observers) {
        if (observer.handler)
            observer.handler(changes);
    }
}
