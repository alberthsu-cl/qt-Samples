# ADR-002: PlaybackSession Is the Playback-State Authority

Status: Accepted

Date: 2026-09-01

Last revised: 2026-09-02

## Context

The current application stores two mutable playback states in `EditorSession`:
one for a selected source asset and one for the timeline. UI focus chooses which
state `playbackState()` exposes.

Authority then changes by context:

- real source preview takes position from `QMediaPlayer` callbacks;
- timeline preview advances through the MFC timer and
  `SimulatedPlaybackBackend`;
- still images and gaps also use the simulated timer;
- the V1 and A1 players maintain their own media clocks;
- UI selection changes which mutable state is considered active.

This hybrid was useful while migrating the UI incrementally, but it prevents a
clear answer to “what is the current playback position?” It also couples edit
selection to transport identity and makes asynchronous media callbacks capable
of writing into editor-owned state.

## Decision

One `PlaybackSession` is the sole mutable authority for the transport state of
one explicit `PlaybackSource`.

```cpp
using PlaybackSource = std::variant<
    SourceAssetPreview,
    SequencePreview>;

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

struct SourcePreviewStatus {
    MediaAssetId sourceId;
    SourceTimestamp sourceTime;
    SourceTimestamp sourceEndTime;
    SourceCompletionPolicy completionPolicy;
};

struct SequencePreviewStatus {
    SequenceId sequenceId;
    TimelineFrame timelineFrame;
    FrameCount sequenceDuration;
    FrameRate frameRate;
};

using PlaybackContext = std::variant<
    SourcePreviewStatus,
    SequencePreviewStatus>;

struct PlaybackStatus {
    PlaybackSessionId sessionId;
    PlaybackGeneration generation;
    StatusSequenceNumber statusSeq;
    PlaybackContext context;
    PlaybackPhase phase;
    int ratePercent = 100;
    std::optional<PlaybackError> error;
    std::optional<PlaybackCommandId> lastAppliedCommandId;
};

struct PlaybackCommandRejected {
    PlaybackCommandId id;
    PlaybackRejectReason reason;
};
```

`PlaybackContext` is derived from the active `PlaybackSource` plus resolved
media metadata. Their alternatives correspond one-to-one: a source-asset
command produces `SourcePreviewStatus`, and a sequence command produces
`SequencePreviewStatus`. This makes a source identity paired with a timeline
position, or a sequence identity paired with a source timestamp, impossible to
represent in one status value.

Source preview covers the whole asset in milestone 1 and therefore starts at
source-time zero. `sourceEndTime` is an instant, not a duration disguised as a
`SourceTimestamp`. A future trimmed-source preview must add an explicit
`sourceStartTime`.

`SourceTimestamp` deliberately has no general arithmetic in ADR-001. The UI
obtains source-slider progress only through the named boundary conversion:

```cpp
SourceTimestamp sourceTimeZero();

int sourceProgressPermille(
    SourceTimestamp position,
    SourceTimestamp end);
```

`sourceTimeZero()` is the named construction boundary for the source origin.
The progress result is clamped to `[0, 1000]` and is zero when `end` equals that
origin. The conversion implementation may access framework units inside the
UI/media adapter, but raw source-time arithmetic does not escape that boundary.
A display-only legacy frame value may likewise be produced by a named, lossy UI
adapter; it never becomes engine input.

`error` is engaged if and only if `phase == PlaybackPhase::Failed`. The
position in `PlaybackContext` is the authoritative transport position. The
frame most recently presented by a renderer is intentionally absent and is
specified by ADR-003.

The session owns:

- active playback source;
- current phase;
- authoritative timeline/source position;
- playback rate;
- master-clock anchor;
- active snapshot revision;
- current generation;
- end and failure state.

The session receives an injected `IPlaybackClock`. Production may provide a
steady-clock implementation; deterministic tests provide a controllable fake.
Session code does not call `std::chrono::steady_clock` directly.

Sequence playback uses a `(MasterClockTime, SequenceTime)` anchor. ADR-004
defines advancement, rate conversion, and re-anchoring. For milestone-1 source
preview, authoritative `SourceTimestamp` progress is adopted from backend
observations only after the session validates their session and generation.
ADR-004 may later unify source and sequence clock advancement without changing
this ownership boundary.

All state-changing playback commands are serialized by the playback engine
thread:

```cpp
using PlaybackCommand = std::variant<
    OpenSource,
    InstallSnapshot,
    Play,
    Pause,
    Stop,
    Seek,
    SetRate,
    Shutdown>;
```

No UI object, editor controller, decoder, player callback, or timer may mutate
the session directly. They submit commands or observations to the engine.

Command submission never waits for media work. Each queued command receives a
`PlaybackCommandId`. Queue-closure rejection may be returned immediately;
phase-dependent rejection is published as `PlaybackCommandRejected` on the
same ordered engine-event channel as status. An accepted command is
acknowledged by a later status whose `lastAppliedCommandId` identifies it.

`PlaybackRejectReason` is an opaque engine-level reason in this ADR. Detailed
media and decoder error taxonomy belongs to ADR-003 and its decoder contract.

The engine publishes immutable `PlaybackStatus` values. UI code may cache the
latest status for painting controls, but that cache is not authoritative and
cannot be written back as engine state.

`statusSeq` increases monotonically within one `PlaybackSessionId` and restarts
for a new session. A consumer accepts only a status newer than the last status
it accepted for that session. A new session ID resets that comparison.

## Separation from EditorSession

`EditorSession` remains authoritative for:

- editable project and sequence data;
- media-library state;
- clip selection and editing focus;
- undo/redo history;
- workspace-facing edit state.

`PlaybackSession` remains authoritative for:

- running/paused/stopped state;
- playback time and rate;
- source/sequence being previewed;
- seek and preroll generation;
- playback completion or failure.

An editor action may cause a playback command—for example, selecting a source
asset may submit `OpenSource`. This is an explicit request. Merely changing
keyboard focus cannot switch the engine's authoritative state implicitly.

Selecting a timeline clip is not transport intent. It changes the editor's
presentation target only: the playhead and playback phase remain unchanged and
no playback command is submitted. While playback is inactive, presentation may
therefore show the selected clip independently of the authoritative playhead.
ADR-003 defines how that non-playhead frame is requested and published. During
active transport, playback context comes only from `PlaybackStatus`; UI focus
never changes playback authority.

Selecting a source asset is different because it explicitly requests source
preview. It submits `OpenSource` and requests the source's first frame. With no
later transport command, it finishes in `Stopped`. The legacy
`leavePausedTimelinePlaybackForEditing()` workaround is not called on the new
path.

## Player and decoder callbacks are observations

`QMediaPlayer`, future decoder workers, the audio device, and the compositor
report observations such as:

- source loaded;
- decoded frame ready;
- audio clock position;
- end of source;
- media failure.

The session decides whether an observation belongs to its current source and
generation and whether it causes a state transition. A media backend never
publishes a new authoritative UI position on its own.

An observation whose session or generation does not match is discarded. A
duplicate observation, or one that does not imply a new transition, is
idempotent.

In source preview, a `QMediaPlayer::positionChanged` callback may report a
candidate `SourceTimestamp`; after identity validation, the session may adopt
it as the authoritative source position for milestone 1. In sequence preview,
player callbacks remain media observations and never replace the session's
master-clock-derived sequence position.

## UI timers are presentation heartbeats only

The MFC playback timer will no longer advance one timeline frame. A UI timer or
posted message may request repainting of the playhead and transport controls,
but it samples the latest `PlaybackStatus`. Its heartbeat interval is constant;
playback rate does not alter UI timer frequency.

If UI repainting is delayed, playback time continues according to the engine's
master clock. When the UI resumes, it displays the newest status rather than
replaying missed timer ticks.

On the new path, `IPlaybackBackend::executeCommand()` no longer returns a timer
directive and `MainFrame::applyPlaybackClockAction()` is not used. Timer
ownership comes from the presentation adapter, not from a playback-command
result.

## Session identity and generation

`PlaybackSessionId` identifies the lifetime of one playback session.
`PlaybackGeneration` identifies the current asynchronous epoch within that
session.

The generation advances when operations invalidate previously requested media,
including:

- every accepted seek, including each repeated scrubbing request;
- playback-source replacement;
- sequence-snapshot replacement;
- stop/shutdown when pending work must be discarded.

Session and generation identity travel with every asynchronous request, result,
and UI publication. ADR-003 will define snapshot and generation invalidation in
detail.

## State-transition ownership

Only `PlaybackSession` implements the playback state machine. Adapters translate
framework events into engine observations; they do not reproduce transition
logic.

`Stopped` means the context is positioned at its defined start. `Paused` means
the authoritative transport position is frozen; it does not claim that a
renderer has already presented the requested frame. Renderer acknowledgement
belongs to ADR-003.

The complete milestone-1 command policy is:

| Command | Accepted phases and transition | Other phases |
| --- | --- | --- |
| `OpenSource` | From every phase except after shutdown: increment generation, replace the context, and enter `Seeking` while loading and seeking to source-time zero. Pending intent initially is `Stopped`; without a later command, first-frame readiness completes in `Stopped`. `Play` changes pending intent to `Playing`, `Pause` changes it to `Paused`, and `Seek` replaces the target while preserving the latest intent, using their normal in-flight rules below. From `Failed`, this is a recovery path and clears the error on success. The command carries `SourceCompletionPolicy`, which is echoed in status. | Rejected after shutdown. |
| `InstallSnapshot` | From `Stopped`, `Paused`, or `Playing`: increment generation, replace the snapshot, preserve and clamp the requested position, and preserve transport intent. Playing returns through preroll to `Playing`; Paused returns to `Paused`; Stopped returns to `Stopped`. During `Seeking` or `Prerolling`, replace the in-flight request, increment generation, and preserve its pending Play/Pause intent. From `Failed`, clear the error, install the snapshot at sequence start, and enter `Stopped`. | Rejected after shutdown. |
| `Play` | `Stopped` or `Paused` enters `Prerolling` and then `Playing`. `Playing` is an accepted idempotent no-op. During `Seeking` or `Prerolling`, pending intent becomes `Playing`. | Rejected from `Failed` or after shutdown. |
| `Pause` | `Playing` enters `Paused`. `Stopped` or `Paused` is an accepted idempotent no-op. During `Seeking` or `Prerolling`, pending intent becomes `Paused` and successful completion enters `Paused`. | Rejected from `Failed` or after shutdown. |
| `Seek` | From every non-failed phase, immediately increment generation. A newer seek supersedes older work. From `Playing`, seek and preroll before returning to `Playing`; from `Paused`, return to `Paused`; from `Stopped`, seek and enter `Paused`. During `Seeking` or `Prerolling`, replace the target while preserving pending transport intent. | Rejected from `Failed` or after shutdown. |
| `SetRate` | From every non-failed phase, update the session preference without changing phase. The value persists across source and snapshot replacement. Seeking and prerolling use the latest rate. Playing re-anchoring is specified by ADR-004. | Rejected from `Failed` or after shutdown. |
| `Stop` | From any non-failed active phase, increment generation, invalidate pending work, move to the context start, and enter `Stopped`. `Stopped` is an accepted idempotent no-op. From `Failed`, clear the error, move to the context start, and enter `Stopped`. | Rejected after shutdown. |
| `Shutdown` | From every phase, invalidate outstanding work, acknowledge the command with one final status whose playback phase is unchanged, and then close command acceptance. The acceptance gate is lifecycle state separate from `PlaybackPhase`. | All later commands are rejected because the queue is closed. Thread joining and destruction belong to ADR-005. |

Failure from any asynchronous operation enters `Failed` and publishes its error.
From `Failed`, only `Stop`, `OpenSource`, `InstallSnapshot`, and `Shutdown` are
accepted.

Completion is intentionally asymmetric and comes from explicit policy rather
than UI focus:

Natural completion increments generation and invalidates pending media work;
ADR-003 defines the mechanics of discarding that work.

- source preview with `HoldLastFrame` finishes in `Paused` at
  `sourceEndTime`;
- sequence preview with `ReturnToStart` finishes in `Stopped` at timeline frame
  zero.

## Why this decision

One authority turns playback behavior into a deterministic state machine. It
also makes thread ownership practical: commands enter one serialized queue,
and all state transitions happen on one engine thread.

The editor model and the playback engine can then evolve independently:

- editing produces snapshots and requests;
- playback consumes snapshots and publishes status;
- widgets present status and emit intent;
- decoders provide timestamped media observations.

## Consequences

Positive consequences:

- there is one answer for current phase, position, rate, source, and generation;
- focus and selection no longer choose playback authority;
- UI stalls cannot slow or restart media time;
- decoder callbacks cannot overwrite an intentional seek or source switch;
- state transitions become testable with fake clocks and fake decoders;
- multiple future sessions can coexist because identity is explicit rather
  than global.

Costs and limitations:

- playback commands become asynchronous from the UI's point of view;
- widgets must tolerate a short delay before receiving confirmed status;
- current `EditorSession` playback fields need a staged migration;
- adapters must distinguish commands from observations;
- shutdown must stop the session and join the engine thread explicitly.

## Migration strategy

The compile-time migration flag chooses exactly one mutable authority:

- on the legacy path, the existing controllers own playback state;
- on the new path, `PlaybackSession` is the only mutable playback authority.

On the new path, legacy `EditorSession` playback mutators are not called. The
existing `PlaybackState` may remain only as a read-only UI presentation cache
populated from `PlaybackStatus`; it is never written back to the engine. Any
source-time-to-frame value in that cache is explicitly display-only and lossy.
`PreviewStateResolver` receives active transport context from
`PlaybackStatus`, never from focus. An idle editing-preview override may still
come from selection, as defined under Separation from EditorSession, without
mutating transport.

The migration proceeds in this order:

1. Introduce `PlaybackSession` and test its state machine with fake ports.
2. Add a status adapter that updates existing transport/preview presentation.
3. Route timeline playback commands to the new session behind a compile-time
   feature flag.
4. Stop calling `EditorSession::advancePlaybackFrame()` for the new path.
5. Remove MFC timer advancement from the legacy path after regression parity
   is established; the new path never uses timer advancement.
6. Remove or narrow legacy playback mutation APIs after source preview also
   migrates.

The current `IPlaybackBackend` remains a temporary façade so visible UI code
does not change during this migration. Its new-path command result does not
control the UI timer.

## Alternatives considered

### Keep EditorSession as the playback authority

Rejected because `EditorSession` is an editing/history boundary. Giving it
engine-thread timing responsibility would mix mutable project state with
real-time state and complicate thread safety.

### Make QMediaPlayer the authority

Rejected because a timeline contains still images, gaps, trimmed sources, and
independent V1/A1 media. No single high-level player represents the composed
timeline.

### Keep the MFC timer as the authority

Rejected because UI message delivery is not a media clock. Delayed timer
messages cause drift, and the timer cannot measure audio-device progress.

### Let every backend own its own playback state

Rejected because changing media kind or preview context would change transport
semantics and recreate the current split-authority problem behind interfaces.

## Acceptance criteria

ADR-002 is implemented when:

1. One `PlaybackSession` owns phase, position, rate, source, and generation.
2. Playback-session mutation occurs only on its serialized engine context.
3. UI controls submit commands and display immutable status publications.
4. `EditorSession` and UI focus do not select or advance authoritative playback
   state.
5. Player/decoder callbacks enter the engine as identified observations.
6. The MFC/Qt UI timer does not advance timeline time.
7. Pause, seek, resume, stop, completion, and failure transitions have pure
   state-machine tests.
8. A stale observation cannot change session state after source or generation
   replacement.
9. The existing Qt widget tests pass unmodified through the temporary
   backend/status adapters, including their current visible behavior.
10. `PlaybackContext` matches the active `PlaybackSource`, and `error` is
    engaged if and only if phase is `Failed`.
11. An invalid command produces a rejection carrying its command ID and a
    reason; from `Failed`, only the four documented recovery/exit commands are
    accepted.
12. Duplicate observations are idempotent, and consumers discard non-newer
    `statusSeq` publications within a session.
13. With the new path enabled, tests prove that no legacy `EditorSession`
    playback mutator is called.
14. Selecting a timeline clip while paused leaves phase and playhead unchanged
    and changes only the editing-preview presentation target.
15. Source-progress tests cover zero end time and clamping below zero and above
    1000 through the named conversion.

## Learning focus

Ownership answers who stores and mutates a value. Authority answers which value
is considered correct when components disagree. ADR-002 deliberately gives
both responsibilities for transport state to `PlaybackSession`; every other
component either requests a change or reports an observation.
