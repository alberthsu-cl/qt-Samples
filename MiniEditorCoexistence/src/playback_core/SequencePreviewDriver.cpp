#include "SequencePreviewDriver.h"

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
                                           bool transportJustRepositioned,
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
    if (transportJustRepositioned)
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

    // A1 first, and unconditionally: the two tracks have their own clip
    // boundaries, so a V1 gap must not silence audio that is still running,
    // and an early return on the video side must not skip the audio lane.
    driveAudioLane(resolved, transportJustRepositioned, outcome);

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
    } else if (transportJustRepositioned) {
        // Same clip, but the playhead jumped. Only meaningful for a lane a
        // continuous player is actually running: while paused the frame comes
        // from the scheduler below, which carries its own source time.
        if (status.phase == PlaybackPhase::Playing)
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
