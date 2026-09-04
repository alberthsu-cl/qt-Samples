#include "SequencePreviewDriver.h"

#include <cstdlib>

namespace mini_editor::playback_core {

SequencePreviewDriver::SequencePreviewDriver(PreviewPresentationCoordinator &coordinator,
                                             VideoWorkScheduler &scheduler,
                                             const IPlaybackClock &clock)
    : coordinator_(coordinator)
    , scheduler_(scheduler)
    , clock_(clock)
{
}

void SequencePreviewDriver::installSnapshot(SequencePlaybackSnapshotPtr snapshot)
{
    if (!snapshot)
        return;

    snapshot_ = std::move(snapshot);
    openClipId_.reset();
    openAudioClipId_.reset();
    // New content: no previous position to be continuous with.
    lastDrivenFrame_.reset();
    lastDrivenClock_.reset();
}

bool SequencePreviewDriver::playheadJumped(const PlaybackStatus &status,
                                           TimelineFrame timelineFrame)
{
    // Two frames of slack: a sampling tick lands mid-frame, so the resolved
    // frame is routinely one off the arithmetic, and a seek worth chasing is
    // never that small.
    constexpr std::int64_t kToleranceFrames = 2;

    const bool continuous = lastDrivenFrame_ && lastDrivenClock_ && snapshot_
        && [&] {
            const ClockDuration elapsed = clock_.now() - *lastDrivenClock_;
            // A parked transport advances by nothing, so any movement at all
            // is a jump.
            const std::int64_t advanced = status.phase == PlaybackPhase::Playing
                ? frameAtSequenceTime(
                      SequenceTime::fromMicroseconds(
                          sequenceElapsedFor(elapsed, status.ratePercent)
                              .microsecondsForAdapter()),
                      snapshot_->frameRate).frameNumber()
                : 0;
            const std::int64_t expected = lastDrivenFrame_->frameNumber() + advanced;
            return std::llabs(timelineFrame.frameNumber() - expected) <= kToleranceFrames;
        }();

    lastDrivenFrame_ = timelineFrame;
    lastDrivenClock_ = clock_.now();
    return !continuous;
}

std::optional<int> SequencePreviewDriver::openClipId() const
{
    return openClipId_;
}

std::optional<int> SequencePreviewDriver::openAudioClipId() const
{
    return openAudioClipId_;
}

void SequencePreviewDriver::driveAudioLane(const ResolvedSnapshotFrame &resolved,
                                           bool playheadJumped,
                                           PreviewDriveOutcome &outcome)
{
    if (!resolved.audio || resolved.audio->availability != MediaAvailability::Available) {
        openAudioClipId_.reset();
        outcome.silenceAudio = true;
        return;
    }

    const ResolvedSnapshotClip &clip = *resolved.audio;
    // A1 is a continuous lane with no still-image case and no request/response
    // decode: ADR-003's bounded latest-wins policy is a video policy, and
    // ADR-004 defers audio buffering to a later milestone. All this lane needs
    // is which file, where in it, and how loud.
    outcome.audioLevelPercent = clip.fadeGainPercent;
    if (openAudioClipId_ != clip.clipId) {
        openAudioClipId_ = clip.clipId;
        outcome.openAudioClip = PreviewSourceChange{
            clip.clipId, clip.mediaAssetId, clip.mediaKind,
            clip.immutableSourceLocator, clip.sourceTime
        };
        return;
    }

    // Same clip, but the playhead jumped: this lane's player is still running
    // from where it was.
    if (playheadJumped)
        outcome.repositionAudioTo = clip.sourceTime;
}

PreviewDriveOutcome SequencePreviewDriver::notifyPlaybackStatus(const PlaybackStatus &status,
                                                                bool transportJustRepositioned)
{
    coordinator_.notifyPlaybackStatus(status, transportJustRepositioned);

    const auto *sequence = std::get_if<SequencePreviewStatus>(&status.context);
    if (!snapshot_ || sequence == nullptr)
        return {};

    // ADR-003: on failure the last accepted frame is retained. Switching
    // sources here would replace a picture with a blank one at exactly the
    // moment the user needs to see what was playing when it broke.
    if (status.phase == PlaybackPhase::Failed)
        return {};

    // A status describing content other than the installed snapshot resolves
    // nothing: the two are about to agree again, and guessing in between is
    // how a wrong-clip frame reaches the viewport.
    if (sequence->sequenceId != snapshot_->sequenceId
        || sequence->revision != snapshot_->revision) {
        return {};
    }

    const ResolvedSnapshotFrame resolved =
        SnapshotTimelineResolver::resolve(*snapshot_, sequence->timelineFrame);

    PreviewDriveOutcome outcome;
    outcome.isVideoTrackMuted = snapshot_->isVideoTrackMuted;

    // Whether the playhead jumped is a question about the position, not about
    // which command produced this status -- so it survives a status event and
    // a sampling tick racing each other.
    const bool jumped = playheadJumped(status, sequence->timelineFrame);

    // A1 first, and unconditionally: the two tracks have their own clip
    // boundaries, so a V1 gap must not silence audio that is still running,
    // and an early return on the video side must not skip the audio lane.
    driveAudioLane(resolved, jumped, outcome);

    if (!resolved.video || resolved.video->availability != MediaAvailability::Available) {
        // A gap, the tail past the last clip, or a file the snapshot could not
        // find. None of these is a failure -- there is simply nothing to show.
        openClipId_.reset();
        outcome.showNothing = true;
        return outcome;
    }

    const ResolvedSnapshotClip &clip = *resolved.video;
    if (openClipId_ != clip.clipId) {
        openClipId_ = clip.clipId;
        outcome.openClip = PreviewSourceChange{
            clip.clipId, clip.mediaAssetId, clip.mediaKind,
            clip.immutableSourceLocator, clip.sourceTime
        };
    } else if (jumped && status.phase == PlaybackPhase::Playing) {
        // Same clip, but the playhead jumped. Only meaningful while a
        // continuous player is actually running: while paused the frame comes
        // from the scheduler below, which carries its own source time.
        outcome.repositionVideoTo = clip.sourceTime;
    }

    // Bounded latest-wins decode is the scrubbing case. While Playing, the
    // adapter's continuous player is already producing frames, so asking a
    // request/response decoder for the same ones would put two competing
    // producers on one viewport. A still has no source time to decode at, so
    // it is shown, not decoded -- which is what keeps a still from reaching a
    // video decoder and coming back as an error.
    const bool playerIsAlreadyProducingFrames = status.phase == PlaybackPhase::Playing;
    if (!playerIsAlreadyProducingFrames && clip.sourceTime) {
        if (const std::optional<FramePresentationRequest> request = coordinator_.currentRequest()) {
            scheduler_.requestFrame(
                VideoDecodeRequest{
                    SequenceWorkIdentity{
                        PlaybackWorkIdentity{status.sessionId, status.generation},
                        sequence->sequenceId, sequence->revision
                    },
                    clip.mediaAssetId, *clip.sourceTime, clock_.now()
                },
                *request);
        }
    }
    return outcome;
}

} // namespace mini_editor::playback_core
