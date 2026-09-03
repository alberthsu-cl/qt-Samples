#pragma once

#include "MediaTime.h"
#include "ProjectRuntime.h"
#include "SequencePlaybackSnapshot.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace mini_editor::playback_core {

// ADR-002 types a source-asset identity as MediaAssetId, but no such type
// exists yet -- the codebase uses a plain int for media-asset ids everywhere
// (PlaybackMediaDescriptor::mediaAssetId, EditorSession's clipboard asset id).
// This is a thin explicit wrapper over that same existing int space, not a
// fresh runtime-generated identity like ProjectId/SequenceId; it does not
// change any existing plain-int field.
class MediaAssetId final {
public:
    explicit MediaAssetId(int value);

    int value() const;

private:
    int value_;
};

bool operator==(MediaAssetId left, MediaAssetId right);
bool operator!=(MediaAssetId left, MediaAssetId right);

// Runtime identity for one PlaybackSession's lifetime.
class PlaybackSessionId final {
public:
    static PlaybackSessionId create();

    std::uint64_t valueForDiagnostics() const;

private:
    explicit PlaybackSessionId(std::uint64_t value);

    std::uint64_t value_;
};

bool operator==(PlaybackSessionId left, PlaybackSessionId right);
bool operator!=(PlaybackSessionId left, PlaybackSessionId right);

// The current asynchronous epoch within one session. Every accepted seek,
// source replacement, snapshot replacement, and stop/shutdown advances it.
class PlaybackGeneration final {
public:
    static PlaybackGeneration initial();

    std::uint64_t value() const;
    PlaybackGeneration next() const;

private:
    explicit PlaybackGeneration(std::uint64_t value);

    std::uint64_t value_;
};

bool operator==(PlaybackGeneration left, PlaybackGeneration right);
bool operator!=(PlaybackGeneration left, PlaybackGeneration right);

// Identifies one submitted command so its later rejection or acknowledgment
// (PlaybackStatus::lastAppliedCommandId) can be matched back to the caller.
class PlaybackCommandId final {
public:
    static PlaybackCommandId create();

    std::uint64_t valueForDiagnostics() const;

private:
    explicit PlaybackCommandId(std::uint64_t value);

    std::uint64_t value_;
};

bool operator==(PlaybackCommandId left, PlaybackCommandId right);
bool operator!=(PlaybackCommandId left, PlaybackCommandId right);

// Increases monotonically within one PlaybackSessionId and restarts for a
// new session. A consumer accepts only a status newer than the last one it
// accepted for that session.
class StatusSequenceNumber final {
public:
    static StatusSequenceNumber initial();

    std::uint64_t value() const;
    StatusSequenceNumber next() const;

private:
    explicit StatusSequenceNumber(std::uint64_t value);

    std::uint64_t value_;
};

bool operator==(StatusSequenceNumber left, StatusSequenceNumber right);
bool operator!=(StatusSequenceNumber left, StatusSequenceNumber right);
bool operator<(StatusSequenceNumber left, StatusSequenceNumber right);

enum class SourceCompletionPolicy {
    HoldLastFrame,
    ReturnToStart
};

enum class PlaybackPhase {
    Stopped,
    Seeking,
    Prerolling,
    Playing,
    Paused,
    Failed
};

// ADR-002 leaves the reject reason "opaque"; this milestone names the ones
// its own rules can produce. QueueClosed: submitted after Shutdown.
// InvalidForCurrentPhase: rejected by the phase-transition table (e.g. Play
// from Failed). SourceKindMismatch: a Seek/OpenSource/InstallSnapshot payload
// naming the wrong PlaybackSource kind for this session.
enum class PlaybackRejectReason {
    QueueClosed,
    InvalidForCurrentPhase,
    SourceKindMismatch
};

struct PlaybackError final {
    std::string message;
};

struct PlaybackCommandRejected final {
    PlaybackCommandId id;
    PlaybackRejectReason reason;
};

// The two PlaybackSource alternatives. A PlaybackSession is constructed for
// exactly one of these and keeps that kind for its lifetime in this
// milestone; OpenSource switches which asset a SourceAssetPreview session is
// showing, InstallSnapshot switches which content a SequencePreview session
// is showing.
struct SourceAssetPreview final {
    MediaAssetId assetId;
};

struct SequencePreview final {
    SequenceId sequenceId;
};

using PlaybackSource = std::variant<SourceAssetPreview, SequencePreview>;

struct SourcePreviewStatus final {
    MediaAssetId sourceId;
    SourceTimestamp sourceTime;
    SourceTimestamp sourceEndTime;
    SourceCompletionPolicy completionPolicy;
};

struct SequencePreviewStatus final {
    SequenceId sequenceId;
    TimelineFrame timelineFrame;
    FrameCount sequenceDuration;
    FrameRate frameRate;
};

using PlaybackContext = std::variant<SourcePreviewStatus, SequencePreviewStatus>;

struct PlaybackStatus final {
    PlaybackSessionId sessionId;
    PlaybackGeneration generation;
    StatusSequenceNumber statusSeq;
    PlaybackContext context;
    PlaybackPhase phase;
    int ratePercent = 100;
    std::optional<PlaybackError> error;
    std::optional<PlaybackCommandId> lastAppliedCommandId;
};

// sourceTimeZero() is the named construction boundary for the source origin
// (ADR-002); raw source-time arithmetic does not escape it.
SourceTimestamp sourceTimeZero();

// Clamped to [0, 1000]; zero when end equals sourceTimeZero().
int sourceProgressPermille(SourceTimestamp position, SourceTimestamp end);

// The eight ADR-002 engine commands. OpenSource and Seek are only valid for
// the PlaybackSource kind they name; a mismatch is rejected with
// SourceKindMismatch rather than accepted silently.
struct OpenSource final {
    MediaAssetId assetId;
    SourceTimestamp sourceEndTime;
    SourceCompletionPolicy completionPolicy;
};

struct InstallSnapshot final {
    SequencePlaybackSnapshotPtr snapshot;
};

struct Play final {};
struct Pause final {};
struct Stop final {};

struct Seek final {
    std::variant<SourceTimestamp, SequenceTime> target;
};

struct SetRate final {
    int ratePercent = 100;
};

struct Shutdown final {};

using PlaybackCommand = std::variant<
    OpenSource, InstallSnapshot, Play, Pause, Stop, Seek, SetRate, Shutdown>;

} // namespace mini_editor::playback_core
