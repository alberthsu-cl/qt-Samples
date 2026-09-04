#pragma once

#include "EditorCommandController.h"
#include "MfcPlaybackNotificationBridge.h"
#include "QtPlaybackMediaWorker.h"
#include "playback_core/PlaybackEngine.h"
#include "playback_core/PresentationDiagnostics.h"
#include "playback_core/PreviewPresentation.h"
#include "playback_core/SequencePlaybackSnapshot.h"
#include "playback_core/SequencePreviewDriver.h"
#include "playback_core/SteadyPlaybackClock.h"
#include "playback_core/TimelineTransportView.h"
#include "playback_core/VideoWorkScheduler.h"

#include <QObject>
#include <QTimer>

#include <functional>
#include <memory>

class QVideoWidget;
class QVideoSink;

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
// M5-05 moved presentation into the app's own preview panel, on a dedicated
// engine sink the panel owns alongside -- never instead of -- the legacy one
// QtMediaPlaybackBackend drives for source preview. Both before and after
// that change, this router never touches the legacy sink or its
// QMediaPlayer: there are two entirely separate QMediaPlayer instances on two
// entirely separate threads, one per path, which is what makes "at no point
// may both paths hold transport authority for the same preview session" true
// by construction rather than by care. Passing no engine sink keeps M4-06's
// standalone smoke-test window.
//
// M5-02 lifted this class's original "first V1 clip only" scope. Which clip
// is under the playhead, when that is a source switch, and when a gap must
// blank the viewport are all decided by SequencePreviewDriver in the core;
// what is left here is executing those decisions against Qt.
class TimelineEngineRouter final : public QObject {
    Q_OBJECT

public:
    // `engineSurfaceSink` is the preview panel's dedicated engine sink
    // (M5-05, decision B). Pass nullptr to keep M4-06's standalone smoke-test
    // window instead -- the legacy shared sink is never an option here, which
    // is what keeps the two paths from contending for one surface.
    TimelineEngineRouter(HWND notifyTarget, UINT notifyMessage,
                        mini_editor::playback_core::SequenceId sequenceId,
                        QVideoSink *engineSurfaceSink = nullptr);
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

    // Whether timeline preview is the context the user is looking at.
    //
    // Selecting a library asset moves the preview to source-asset playback,
    // which stays entirely on the legacy path (Decision E). Nothing told this
    // router about that, so the panel went on painting the engine's last
    // frame over the legacy source video -- a picture frozen on whatever the
    // timeline was showing, while the legacy path's audio played underneath.
    // The engine's own transport kept running too.
    void setTimelinePreviewActive(bool active);

    // ADR-003's FramePresented moment, forwarded from the preview panel once
    // the frame is actually on its surface. This records a diagnostic and
    // nothing else: it does not touch the clock, the session, or the queue.
    void onEngineFrameCommitted();

    // Told when the panel should paint engine frames rather than the legacy
    // source-preview content. Set once, before the first snapshot.
    using EnginePresentationActiveSink = std::function<void(bool)>;
    void setEnginePresentationActiveSink(EnginePresentationActiveSink sink);

    const mini_editor::playback_core::PresentationDiagnostics &
    presentationDiagnostics() const;

    // M5-04: where each published PlaybackStatus goes to become the UI's
    // read-only painting cache (ADR-002). Always invoked on the GUI thread --
    // both callers, the notification drain and the presentation tick, run
    // there. Set once, before the first snapshot.
    using TransportViewSink =
        std::function<void(const mini_editor::playback_core::TimelineTransportView &)>;
    void setTransportViewSink(TransportViewSink sink);

private:
    void handleEvents(std::vector<mini_editor::playback_core::PlaybackEvent> events);

    // Runs the driver for one status and executes what it asks for.
    void drivePreview(const mini_editor::playback_core::PlaybackStatus &status,
                      bool transportJustRepositioned);

    // A free-running transport publishes no status of its own, so nothing
    // would notice the playhead crossing a clip boundary. This samples while
    // Playing. It is a *presentation* tick, not transport: PlaybackSession's
    // clock still owns position, and this never advances it. That is why
    // M5-09 can retire the MFC timer's transport advancement without
    // affecting this.
    void onPresentationTick();

    // Reads and clears transportRepositionPending_.
    bool takePendingReposition();

    // Called by VideoWorkScheduler when a scrub decode has been composited.
    // May arrive on any thread; marshals to the GUI thread before touching
    // the coordinator.
    void onFrameComposited(mini_editor::playback_core::CompositedVideoFrame frame);

    void setEnginePresentationActive(bool active);

    // Executes the A1 half of a drive outcome (M5-06). Separate from the
    // video half because the two lanes have independent clip boundaries: a
    // V1 source switch must not interrupt audio that is still running.
    void driveAudioLane(const mini_editor::playback_core::PreviewDriveOutcome &outcome,
                        mini_editor::playback_core::PlaybackPhase phase);

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
    mini_editor::playback_core::PreviewPresentationCoordinator coordinator_;
    std::unique_ptr<mini_editor::playback_core::VideoWorkScheduler> scheduler_;
    std::unique_ptr<mini_editor::playback_core::SequencePreviewDriver> driver_;
    QTimer presentationTimer_;
    TransportViewSink transportViewSink_;
    EnginePresentationActiveSink enginePresentationActiveSink_;
    mini_editor::playback_core::PresentationDiagnostics diagnostics_;
    bool isEnginePresentationActive_ = false;
    bool isTimelinePreviewActive_ = true;
    int lastAudioLevelPercent_ = -1;
    bool isAudioSilenced_ = true;
    int lastVideoTrackMuted_ = -1;
    int lastRatePercent_ = -1;
    // Set when this router submits a command that can move the playhead, and
    // consumed by the next drive. M5-02 passed "repositioned" for every
    // engine-driven status because nothing then depended on the distinction;
    // once a reposition means "re-seek the continuous players", passing it
    // loosely re-seeks on every Pause and passing it never leaves a lane
    // playing from where the seek left it.
    bool transportRepositionPending_ = false;
    std::unique_ptr<QVideoWidget> previewWindow_;
    mini_editor::playback_core::PlaybackPhase lastAppliedPhase_ =
        mini_editor::playback_core::PlaybackPhase::Stopped;
};
