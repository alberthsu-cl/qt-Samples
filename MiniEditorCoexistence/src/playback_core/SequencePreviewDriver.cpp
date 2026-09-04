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
}

std::optional<int> SequencePreviewDriver::openClipId() const
{
    return openClipId_;
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
