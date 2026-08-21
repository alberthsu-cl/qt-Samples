#pragma once

#include "ProjectState.h"

#include <cstddef>
#include <functional>
#include <vector>

// Framework-neutral editor state and commands. It knows no MFC window and no
// Qt object. UI frameworks may request changes, but this session owns the
// resulting selection, clip, playback, and timeline state.
class EditorSession final
{
public:
    using StateChangedHandler = std::function<void()>;

    explicit EditorSession(std::size_t assetCount);

    int selectedAssetIndex() const;
    const ClipSettings &selectedClipSettings() const;
    const PlaybackState &playbackState() const;
    const TimelineViewState &timelineViewState() const;

    void selectAsset(int assetIndex);
    void updateSelectedClipSettings(const ClipSettings &settings);
    void handlePlaybackCommand(PlaybackCommand command);
    void advancePlaybackFrame();
    void seekTimeline(int frame);
    void updateTimelineViewState(const TimelineViewState &state);
    void fitTimeline();

    // Restoring settings happens before views are constructed, so this does
    // not notify. The caller performs one initial view refresh afterwards.
    void restoreWorkspaceState(int selectedAssetIndex,
                               const TimelineViewState &timelineViewState);
    void setStateChangedHandler(StateChangedHandler handler);

private:
    void notifyStateChanged();

    std::vector<ClipSettings> clipSettings_;
    int selectedAssetIndex_ = 0;
    PlaybackState playbackState_;
    TimelineViewState timelineViewState_;
    StateChangedHandler stateChangedHandler_;
};
