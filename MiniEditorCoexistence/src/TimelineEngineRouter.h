#pragma once

#include "EditorCommandController.h"
#include "MfcPlaybackNotificationBridge.h"
#include "QtPlaybackMediaWorker.h"
#include "playback_core/PlaybackEngine.h"
#include "playback_core/SequencePlaybackSnapshot.h"
#include "playback_core/SteadyPlaybackClock.h"

#include <QObject>

#include <memory>

class QVideoWidget;

// ADR-002/ADR-007 feature-flagged routing (M4-06): while
// MINI_EDITOR_ENABLE_ENGINE_ROUTING is compiled in, timeline (sequence)
// preview's playback commands are routed through PlaybackEngine and a real
// QtPlaybackMediaWorker instead of the legacy IPlaybackBackend/
// QtMediaPlaybackBackend path. Source-asset preview is completely
// untouched -- it keeps using the existing playbackBackend_, and this class
// never calls a legacy EditorSession playback mutator (handlePlaybackCommand,
// advancePlaybackFrame, seekTimeline, setPlaybackDuration,
// updatePlaybackFromBackend, updatePlaybackRatePercent,
// leavePausedTimelinePlaybackForEditing) -- only PlaybackSession, via
// PlaybackEngine's command queue, owns transport state for the routed path.
//
// This issue proves routing and authority separation, not live visual parity
// in the app's real preview panel -- that is Milestone 5's "compare behavior"
// job. This router owns its own standalone preview window, exactly like the
// M4-04/M4-05 manual smoke test, so it never touches QtMediaPlaybackBackend's
// shared video sink or its QMediaPlayer: there are two entirely separate
// QMediaPlayer instances on two entirely separate threads, one per path,
// which is what makes "at no point may both paths hold transport authority
// for the same preview session" true by construction rather than by care.
//
// Scope note: only the first video clip on V1 is opened for preview.
// Multi-clip timeline resolution (switching source as the playhead crosses
// clip boundaries) needs TimelinePlaybackResolver adapted to consume a
// snapshot, which is ADR-003 migration step 3 and is still unstarted --
// this milestone does not attempt it.
class TimelineEngineRouter final : public QObject {
    Q_OBJECT

public:
    TimelineEngineRouter(HWND notifyTarget, UINT notifyMessage,
                        mini_editor::playback_core::SequenceId sequenceId);
    ~TimelineEngineRouter() override;

    TimelineEngineRouter(const TimelineEngineRouter &) = delete;
    TimelineEngineRouter &operator=(const TimelineEngineRouter &) = delete;

    // Call whenever the timeline's content changes (EditorChange::TimelineClip/
    // AudioMix) and once at startup. A null snapshot is ignored.
    void installSnapshot(mini_editor::playback_core::SequencePlaybackSnapshotPtr snapshot);

    // Translates the four playback EditorIntents into engine commands.
    // MainFrame calls this only while EditorSession::isTimelineFocused() is
    // true; any other intent is a no-op here.
    void applyIntent(EditorIntent intent);

    // Timeline-context seek, mirroring the legacy seekPreviewToCurrentFrame()
    // timeline branch.
    void seekToTimelineFrame(int timelineFrame);

    // Called from MainFrame::OnTimelineEngineNotification.
    void onNotification();

private:
    void handleEvents(std::vector<mini_editor::playback_core::PlaybackEvent> events);

    // What the engine's session last accepted, so a view-only editor change
    // that rebuilds an identical snapshot does not re-open media for content
    // the session will refuse. PlaybackSession remains the authority; this is
    // the same predicate asked one step earlier.
    mini_editor::playback_core::SequencePlaybackSnapshotPtr lastInstalledSnapshot_;
    mini_editor::playback_core::FrameRate currentFrameRate_{30, 1};
    mini_editor::playback_core::FrameCount currentDuration_ =
        mini_editor::playback_core::FrameCount::zero();
    MfcPlaybackNotificationBridge bridge_;
    mini_editor::playback_core::SteadyPlaybackClock clock_;
    std::unique_ptr<mini_editor::playback_core::PlaybackEngine> engine_;
    QtPlaybackMediaWorker worker_;
    std::unique_ptr<QVideoWidget> previewWindow_;
    mini_editor::playback_core::PlaybackPhase lastAppliedPhase_ =
        mini_editor::playback_core::PlaybackPhase::Stopped;
};
