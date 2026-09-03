#pragma once

#include "PlaybackCommand.h"
#include "PreviewPresentation.h"

#include <functional>

namespace mini_editor::playback_core {

// Every sequence decode/composition request/result carries this (ADR-003):
// session+generation identity plus the sequence/revision it resolves
// against. Source-preview work would carry PlaybackWorkIdentity plus a
// MediaAssetId directly instead; this milestone's scheduler only covers
// sequence (timeline) preview, the primary target of this migration --
// source-preview decode requests are a structurally analogous extension,
// not built now.
struct PlaybackWorkIdentity final {
    PlaybackSessionId sessionId;
    PlaybackGeneration generation;
};

struct SequenceWorkIdentity final {
    PlaybackWorkIdentity playback;
    SequenceId sequenceId;
    SequenceRevision revision;
};

bool operator==(const PlaybackWorkIdentity &left, const PlaybackWorkIdentity &right);
bool operator!=(const PlaybackWorkIdentity &left, const PlaybackWorkIdentity &right);
bool operator==(const SequenceWorkIdentity &left, const SequenceWorkIdentity &right);
bool operator!=(const SequenceWorkIdentity &left, const SequenceWorkIdentity &right);

// A placeholder pixel payload. The real buffer type (a QVideoFrame-derived
// or D3D11 resource, converted to something framework-neutral at the
// adapter boundary per ADR-007) is Milestone 4's Qt-adapter concern
// (M4-04); this milestone only needs a comparable, copyable stand-in so the
// scheduling/coalescing policy can be tested deterministically.
struct VideoFrameBuffer final {
    int placeholderPixelChecksum = 0;
};

bool operator==(const VideoFrameBuffer &left, const VideoFrameBuffer &right);
bool operator!=(const VideoFrameBuffer &left, const VideoFrameBuffer &right);

struct VideoDecodeRequest final {
    SequenceWorkIdentity sequence;
    MediaAssetId mediaAssetId;
    SourceTimestamp sourceTime;
    MasterClockTime deadline;
};

bool operator==(const VideoDecodeRequest &left, const VideoDecodeRequest &right);
bool operator!=(const VideoDecodeRequest &left, const VideoDecodeRequest &right);

struct DecodedVideoFrame final {
    SequenceWorkIdentity sequence;
    MediaAssetId mediaAssetId;
    SourceTimestamp sourceTime;
    VideoFrameBuffer buffer;
};

// ADR-007's framework-neutral video decode port. A real implementation
// (M4-04) may deliver onDecoded on any worker thread; the consumer, not the
// service, decides whether a result is still current (ADR-003: "workers
// never decide their own currency").
class IVideoDecodeService {
public:
    virtual ~IVideoDecodeService() = default;

    virtual void requestDecode(VideoDecodeRequest request,
                               std::function<void(DecodedVideoFrame)> onDecoded) = 0;
};

struct PresentedSourcePosition final {
    MediaAssetId mediaAssetId;
    SourceTimestamp sourceTimestamp;
};

struct PresentedSequencePosition final {
    SequenceId sequenceId;
    SequenceRevision sequenceRevision;
    TimelineFrame timelineFrame;
};

bool operator==(const PresentedSourcePosition &left, const PresentedSourcePosition &right);
bool operator!=(const PresentedSourcePosition &left, const PresentedSourcePosition &right);
bool operator==(const PresentedSequencePosition &left, const PresentedSequencePosition &right);
bool operator!=(const PresentedSequencePosition &left, const PresentedSequencePosition &right);

using PresentedPosition = std::variant<PresentedSourcePosition, PresentedSequencePosition>;

struct CompositedVideoFrame final {
    PresentationSessionId presentationSessionId;
    PresentationRequestId requestId;
    PresentationAuthority authority;
    PresentedPosition position;
    VideoFrameBuffer buffer;
};

// ADR-007's framework-neutral compositor port. Composition is a separate
// step from decoding (target-architecture decision 9): it turns one decoded
// frame plus the presentation request it satisfies into one immutable,
// presentation-identified result.
class IVideoCompositor {
public:
    virtual ~IVideoCompositor() = default;

    virtual void composite(DecodedVideoFrame frame, FramePresentationRequest request,
                           std::function<void(CompositedVideoFrame)> onComposited) = 0;
};

// The audio-side port shape, declared for completeness (ADR-007 names it
// alongside the video ports) but not scheduled by this milestone: ADR-004's
// audio buffering/underflow policy is different in kind from ADR-003's
// bounded latest-wins VIDEO policy, and needs a real audio device to be
// meaningful. That scheduling is Milestone 4's Qt-adapter issue (M4-04).
struct AudioDecodeRequest final {
    SequenceWorkIdentity sequence;
    MediaAssetId mediaAssetId;
    SourceTimestamp sourceTime;
};

struct DecodedAudioSamples final {
    SequenceWorkIdentity sequence;
    MediaAssetId mediaAssetId;
    SourceTimestamp sourceTime;
};

class IAudioDecodeService {
public:
    virtual ~IAudioDecodeService() = default;

    virtual void requestDecode(AudioDecodeRequest request,
                               std::function<void(DecodedAudioSamples)> onDecoded) = 0;
};

} // namespace mini_editor::playback_core
