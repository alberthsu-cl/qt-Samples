#include "VideoWork.h"

namespace mini_editor::playback_core {

bool operator==(const PlaybackWorkIdentity &left, const PlaybackWorkIdentity &right)
{
    return left.sessionId == right.sessionId && left.generation == right.generation;
}
bool operator!=(const PlaybackWorkIdentity &left, const PlaybackWorkIdentity &right)
{
    return !(left == right);
}

bool operator==(const SequenceWorkIdentity &left, const SequenceWorkIdentity &right)
{
    return left.playback == right.playback
        && left.sequenceId == right.sequenceId
        && left.revision == right.revision;
}
bool operator!=(const SequenceWorkIdentity &left, const SequenceWorkIdentity &right)
{
    return !(left == right);
}

bool operator==(const VideoFrameBuffer &left, const VideoFrameBuffer &right)
{
    return left.placeholderPixelChecksum == right.placeholderPixelChecksum
        && left.platformHandle == right.platformHandle;
}
bool operator!=(const VideoFrameBuffer &left, const VideoFrameBuffer &right)
{
    return !(left == right);
}

bool operator==(const VideoDecodeRequest &left, const VideoDecodeRequest &right)
{
    return left.sequence == right.sequence
        && left.mediaAssetId == right.mediaAssetId
        && left.sourceTime == right.sourceTime
        && left.deadline == right.deadline;
}
bool operator!=(const VideoDecodeRequest &left, const VideoDecodeRequest &right)
{
    return !(left == right);
}

bool operator==(const PresentedSourcePosition &left, const PresentedSourcePosition &right)
{
    return left.mediaAssetId == right.mediaAssetId && left.sourceTimestamp == right.sourceTimestamp;
}
bool operator!=(const PresentedSourcePosition &left, const PresentedSourcePosition &right)
{
    return !(left == right);
}

bool operator==(const PresentedSequencePosition &left, const PresentedSequencePosition &right)
{
    return left.sequenceId == right.sequenceId
        && left.sequenceRevision == right.sequenceRevision
        && left.timelineFrame == right.timelineFrame;
}
bool operator!=(const PresentedSequencePosition &left, const PresentedSequencePosition &right)
{
    return !(left == right);
}

} // namespace mini_editor::playback_core
