#pragma once

#include "playback_core/PlaybackCommand.h"
#include "playback_core/VideoWork.h"

#include <QObject>
#include <QString>
#include <QThread>

#include <memory>

class QMediaPlayer;
class QVideoSink;
class QAudioOutput;

// The real Qt Multimedia adapter for Milestone 4 (ADR-005/ADR-007): one
// dedicated worker thread owns one QMediaPlayer/QVideoSink/QAudioOutput, so
// none of it runs on the GUI thread and none of it is constructed there
// either -- they are created lazily the first time the worker thread's own
// event loop runs, never on the thread that constructs this object.
//
// QMediaPlayer is a continuous player, not a "decode one frame on request"
// API, so this adapter's IVideoDecodeService/IVideoCompositor implementation
// only covers the paused/scrubbing case honestly: requestDecode() seeks to a
// position and reports back the one resulting frame. While playing, this
// adapter does not use the request/response decode API at all -- play()
// starts real playback and framePlayed() forwards every frame QVideoSink
// naturally produces, which already implements ADR-004's "present the
// newest frame, drop superseded ones" policy more directly than a synthetic
// per-frame decode-request loop over a continuous player would.
//
// This is the first place real Qt Multimedia behavior meets the new engine
// (see the M4-04 issue and the Milestone 4 plan doc): the ownership/identity
// rules here are unit-testable in principle, but whether decode actually
// looks and sounds right is not -- that needs a real media file and a human
// watching/listening.
class QtPlaybackMediaWorker final : public QObject {
    Q_OBJECT

public:
    // Takes no parent: moveToThread() (used internally to run this object's
    // media work off the GUI thread) requires an unparented object, and
    // ADR-005 says no engine QObject uses a UI parent regardless.
    QtPlaybackMediaWorker();
    ~QtPlaybackMediaWorker() override;

    QtPlaybackMediaWorker(const QtPlaybackMediaWorker &) = delete;
    QtPlaybackMediaWorker &operator=(const QtPlaybackMediaWorker &) = delete;

    // Every one of these is callable from any thread; each marshals onto the
    // worker thread and returns immediately (ADR-005: the GUI thread must
    // not block waiting for a frame, buffer, or engine command).
    //
    // filePath is a direct path for this milestone's smoke test. Resolving
    // a MediaAssetId to a file lives in the editor-side MediaLibrary, which
    // this adapter does not depend on -- production routing (Milestone
    // 4's M4-06 / Milestone 5) is expected to supply a resolved path the
    // same way the existing QtMediaPlaybackBackend already does.
    void openSource(const QString &filePath);
    void play();
    void pause();
    void stop();
    void seekTo(mini_editor::playback_core::SourceTimestamp position);
    void setRatePercent(int ratePercent);

    // M5-06, decision C: the A1 lane gets its own QMediaPlayer/QAudioOutput
    // pair on this same worker thread, mirroring the legacy two-player
    // topology. A separate pair rather than a second track on one player is
    // what lets the audio lane be scheduled without waiting on video
    // (ADR-004): V1 can be re-opened at a clip boundary while A1 keeps
    // playing straight through its own clip.
    //
    // Audio-master clock selection stays deferred -- the monotonic
    // steady_clock is still the master (ADR-004's milestone-1 exception), so
    // this lane follows the transport rather than defining it.
    void openAudioSource(const QString &filePath);
    void playAudio();
    void pauseAudio();
    void stopAudio();
    void seekAudioTo(mini_editor::playback_core::SourceTimestamp position);
    void setAudioRatePercent(int ratePercent);

    // The A1 clip's fade ramp, as a percentage. Applied on top of the same
    // base volume the legacy path uses, so a fade sounds identical on both.
    void setAudioLevelPercent(int levelPercent);
    // Silences the lane without tearing its player down: seeking while
    // stopped or paused must never leak a short packet through the speakers.
    void setAudioMuted(bool muted);
    // The snapshot's timelineAudioMix.isVideoTrackMuted, applied to the video
    // player's own audio output.
    void setVideoTrackAudioMuted(bool muted);

    mini_editor::playback_core::IVideoDecodeService &videoDecodeService();
    mini_editor::playback_core::IVideoCompositor &videoCompositor();

    // Smoke-test only: lets QMediaPlayer output directly to a caller-owned
    // QVideoSink (e.g. a QVideoWidget's own) instead of the private,
    // headless one this worker creates by default -- so a human can
    // actually see decoded frames. Must be called before the first
    // openSource(); no production code path uses this. `sink` must outlive
    // this worker. Callable from any thread; marshals onto the worker
    // thread like every other method here.
    void attachExternalVideoOutput(QVideoSink *sink);

signals:
    // Emitted on the worker thread. A caller marshals this to whatever
    // identity-validated PlaybackSession::reportFailure() call it needs --
    // this class does not know about PlaybackSession.
    void mediaErrorOccurred(QString message);

    // Emitted on the worker thread whenever QVideoSink receives a new frame
    // while playing (see class comment above).
    void framePlayed(mini_editor::playback_core::VideoFrameBuffer frame);

private:
    class VideoDecodeServiceImpl;
    class VideoCompositorImpl;

    void ensureMediaObjectsExist();
    void ensureAudioLaneObjectsExist();

    QThread thread_;
    QMediaPlayer *player_ = nullptr;
    QVideoSink *videoSink_ = nullptr;
    QAudioOutput *audioOutput_ = nullptr;
    QMediaPlayer *audioLanePlayer_ = nullptr;
    QAudioOutput *audioLaneOutput_ = nullptr;
    int audioLaneLevelPercent_ = 100;
    bool isVideoTrackAudioMuted_ = false;
    QVideoSink *externalVideoSink_ = nullptr; // set only via attachExternalVideoOutput(), read only on thread_.
    std::unique_ptr<VideoDecodeServiceImpl> videoDecodeService_;
    std::unique_ptr<VideoCompositorImpl> videoCompositor_;
};

