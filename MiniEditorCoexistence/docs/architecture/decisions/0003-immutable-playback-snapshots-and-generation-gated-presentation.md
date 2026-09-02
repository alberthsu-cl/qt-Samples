# ADR-003: Immutable Playback Snapshots and Generation-Gated Presentation

Status: Proposed

Date: 2026-09-02

## Context

The current preview path repeatedly resolves mutable `TimelineModel` and
`MediaLibrary` containers while edits, undo/redo, selection, decoder callbacks,
and playback notifications can occur. Its video seek tracker already rejects an
old result by request ID, but that protection is local to one backend path.

ADR-001 established strong time domains. ADR-002 made `PlaybackSession` the sole
transport authority and established a generation for every asynchronous epoch.
The next contract must answer four related questions before decode-service and
threading issues can be implemented safely:

1. What exact, immutable sequence value does playback consume?
2. When does a sequence revision or playback generation change?
3. How are late decode, composition, and frame-publication results rejected?
4. How can clip selection request a non-playhead frame without mutating
   transport authority?

## Decision

Playback consumes a self-contained immutable `SequencePlaybackSnapshot`.
Asynchronous media work is accepted only when all relevant identity values still
match. A separate `PreviewPresentationCoordinator` owns the desired frame for
one preview viewport; it never owns transport state.

## Immutable sequence snapshot

The framework-neutral snapshot contains all playback-affecting sequence data:

```cpp
struct PlaybackClip {
    ClipId clipId;
    MediaAssetId mediaAssetId;
    TimelineTrackType trackType;
    TimelineFrame startFrame;
    FrameCount duration;
    std::optional<SourceTimestamp> sourceIn;
    ClipSettings settings;
};

struct SequencePlaybackSnapshot {
    SequenceId sequenceId;
    SequenceRevision revision;
    FrameRate frameRate;
    FrameCount duration;
    std::vector<PlaybackMediaDescriptor> media;
    std::vector<PlaybackClip> videoClips;
    std::vector<PlaybackClip> audioClips;
    TimelineAudioMixState audioMix;
};
```

`PlaybackMediaDescriptor` is a value describing the media kind, immutable source
locator, optional source extent, availability, and capabilities required by
playback. It contains no player, decoder, widget, callback, or reference to a
media-library row. A still image has no running `sourceIn`; video and audio do.
Detailed decoder error and encoded-media metadata evolve behind this descriptor
without exposing Qt types.

The snapshot contains descriptors only for assets referenced by its clips.
Importing, removing, or changing an unreferenced library asset therefore does
not change the sequence revision or invalidate timeline work.

A library entry whose local file is missing becomes an explicit unavailable
descriptor; this does not make the snapshot structurally partial. Attempting to
resolve it produces the deterministic media-failure/placeholder policy owned by
the media adapter. A clip whose media identity has no descriptor at all is a
snapshot-build error.

Clip vectors are normalized into deterministic timeline order. Milestone 1
retains one non-overlapping V1 vector and one non-overlapping A1 vector, but the
snapshot boundary does not depend on their UI controls.

All members are values or immutable shared resources. The builder publishes the
snapshot as:

```cpp
using SequencePlaybackSnapshotPtr =
    std::shared_ptr<const SequencePlaybackSnapshot>;

using SnapshotBuildResult = std::variant<
    SequencePlaybackSnapshotPtr,
    SnapshotBuildError>;
```

The `const` pointee is part of the contract. Playback code cannot cast it away,
retain references to editor containers, or repair an invalid snapshot after
publication.

## Snapshot construction is an editor-thread transaction

The UI/editor thread builds a candidate entirely from one completed editor
state. It validates the candidate before publication:

- sequence, clip, and media identities are valid and unique in their domains;
- every clip references a descriptor contained by the snapshot;
- `FrameRate` is valid;
- clip duration is positive and its start is nonnegative;
- video and audio vectors satisfy the milestone track-overlap policy;
- video/audio `sourceIn` and extent are valid for their descriptor;
- still images have no source-time progression;
- fade and effect settings satisfy existing edit-policy bounds;
- snapshot duration equals the greatest clip end, or zero for an empty
  sequence.

Validation failure returns `SnapshotBuildError`; no partial snapshot is
installed and the previous accepted snapshot remains usable.

Building does not hold an engine lock and installing does not read editor state.
The only cross-boundary value is `SequencePlaybackSnapshotPtr`.

## Sequence revision

`SequenceRevision` is a monotonic, at-least-64-bit value scoped to one
`SequenceId`. It is runtime identity and is not persisted in the current project
format.

Every committed edit or referenced-media metadata change that can change
resolved video, audio, timing, composition, or audio routing builds a new
snapshot with a strictly newer revision. This includes undo and redo: restoring
old content creates a new revision rather than reusing its historical number.

Selection, playhead motion, panel visibility, timeline zoom, and other view-only
changes do not create a new sequence revision. Reloading a project creates new
runtime sequence identity and revision state.

For the same installed `SequenceId`, `InstallSnapshot` rejects a revision that
is not strictly newer. A sequence not yet installed in this playback session may
begin at any valid revision. Duplicate or out-of-order installs therefore cannot
roll playback content backward accidentally.

## Snapshot installation

`InstallSnapshot` transfers shared ownership of a validated snapshot to the
playback engine command queue. On the engine thread, acceptance is atomic:

1. validate command/session state and snapshot identity;
2. advance `PlaybackGeneration` before scheduling any replacement work;
3. replace the active snapshot pointer;
4. preserve and clamp the requested timeline position;
5. logically invalidate work from the previous generation;
6. seek or preroll according to ADR-002's pending transport intent.

The old snapshot remains alive only through immutable shared references already
held by bounded in-flight work. It is never modified to resemble the new one.

An empty snapshot is valid. Its only timeline position is frame zero, it resolves
to a gap, and Play completes immediately according to ADR-002's sequence
completion policy.

## Work identity and stale-result rejection

`PlaybackGeneration` is monotonic and at least 64 bits within one
`PlaybackSessionId`. It must not wrap during a session. If exhaustion were ever
reached, the session fails rather than reusing an identity.

Every request and result carries a complete immutable identity:

```cpp
struct PlaybackWorkIdentity {
    PlaybackSessionId sessionId;
    PlaybackGeneration generation;
};

struct SequenceWorkIdentity {
    PlaybackWorkIdentity playback;
    SequenceId sequenceId;
    SequenceRevision revision;
};
```

Sequence decode, audio, resolve, composition, and transport-frame publication
carry `SequenceWorkIdentity`. Source-preview work carries
`PlaybackWorkIdentity` plus `MediaAssetId`. Requests also carry the relevant
clip/media identity and strongly typed requested time.

A consumer first compares session and generation in O(1). Sequence work must
also match the active sequence and revision. A mismatch is stale: the consumer
releases its resources and performs no state transition, UI publication,
renderer update, or error fallback.

Correctness never depends on a worker stopping immediately. Cooperative
cancellation is a performance optimization only. Duplicate matching results
remain idempotent under ADR-002.

The generation advances exactly at the invalidation boundaries accepted by
ADR-002: every seek, source replacement, snapshot replacement, stop/shutdown
that invalidates work, and natural completion. Generation assignment happens on
the engine thread; UI and workers never invent it.

## Presentation is separate from transport

One framework-neutral `PreviewPresentationCoordinator` owns the desired visual
target for one preview viewport. It consumes immutable playback status, editor
selection intent, and immutable frame results. It does not advance playback,
change phase, seek the session, or mutate a snapshot.

Each coordinator lifetime receives a unique `PresentationSessionId`. Each
visual request then receives a monotonic, at-least-64-bit
`PresentationRequestId` scoped to that presentation session. Neither identity
may wrap or be reused during its scope:

```cpp
struct TransportPresentationIdentity {
    PlaybackSessionId sessionId;
    PlaybackGeneration generation;
};

struct EditingPresentationIdentity {
    SequenceId sequenceId;
    SequenceRevision revision;
};

using PresentationAuthority = std::variant<
    TransportPresentationIdentity,
    EditingPresentationIdentity>;

struct FramePresentationRequest {
    PresentationSessionId presentationSessionId;
    PresentationRequestId requestId;
    PresentationAuthority authority;
    PresentationTarget target;
};
```

`PresentationTarget` is a domain-safe source timestamp or sequence frame plus
the immutable snapshot/media information required to resolve it. Parallel
source and timeline fields are not used.

The coordinator increments `PresentationRequestId` whenever the desired visual
target changes, including repeated scrubbing, clip selection, source changes,
transport resumption, snapshot replacement, viewport clear, and shutdown. A
result from an older presentation session is stale even if its numeric request
ID equals one in the new session.

The precedence policy is explicit:

- `Playing`, `Seeking`, and `Prerolling` use transport presentation derived from
  `PlaybackStatus` only;
- `Stopped` may show an idle editing target;
- `Paused` keeps transport frozen, but explicit timeline-clip selection may
  temporarily show an editing target without moving the playhead or changing
  phase;
- the next `Play`, `Seek`, `Stop`, `OpenSource`, or `InstallSnapshot` clears that
  editing override and issues a new transport presentation request;
- `Failed` retains the last accepted frame while the error is presented;
- a clear request removes the frame explicitly; absence of a new frame does not
  clear the current one.

This preserves the editor's Split workflow: selecting a clip does not seek the
timeline merely to obtain its first frame.

## Frame publication and renderer acknowledgement

Decoded and composited frame readiness is not the same event as presentation.
The media pipeline publishes an immutable frame candidate:

```cpp
struct CompositedVideoFrame {
    PresentationSessionId presentationSessionId;
    PresentationRequestId requestId;
    PresentationAuthority authority;
    PresentedPosition position;
    VideoFrameBuffer buffer;
};
```

`PresentedPosition` is a variant matching the source or sequence domain.
`VideoFrameBuffer` is logically immutable after publication. The coordinator
accepts a candidate only when its presentation session is current, its request
ID is the newest desired request, and its authority still matches. Transport
candidates additionally must match current session/generation; editing
candidates must match current sequence/revision.

An accepted frame and its position metadata are published to the UI as one
immutable value. The renderer emits `FramePresented` only after it commits that
value to its presentation surface. `FramePresented` carries the same request ID,
authority, and position.

Renderer acknowledgement updates presentation diagnostics only. It does not
advance the master clock or transport position. Decode/composition readiness may
satisfy playback preroll; actual widget/GPU presentation does not block the
engine state machine. ADR-004 defines scheduling deadlines and A/V policy.

## Bounded latest-wins video work

Interactive video work is bounded per playback stream and preview viewport:

- at most one video decode/composition request is in flight;
- at most one newer request is pending;
- a newer valid pending request replaces the older pending request;
- the UI publication queue holds at most one unpresented frame candidate;
- the renderer may retain the last presented frame until replacement or clear.

Replacing pending work releases its snapshot and frame resources immediately.
In-flight work may finish, but identity checks discard it if stale. These bounds
make memory retention independent of scrubbing frequency and keep
per-generation bookkeeping O(1).

Command-queue ownership and shutdown mechanics belong to ADR-005. Audio buffer
depth, latency, underflow, and clock policy belong to ADR-004; audio requests and
results still carry the same session/generation and snapshot identity defined
here.

## Why this decision

The immutable snapshot is a transaction boundary between editing and playback.
Generation is a cheap correctness boundary between old and current work.
Presentation request identity solves a different problem: which valid frame a
viewport currently wants, including an editing frame that is intentionally not
the transport position.

Keeping those three identities separate prevents common accidental couplings:

- undo cannot mutate data being read by a decoder;
- a late seek result cannot replace the newest scrub result;
- importing media cannot regenerate unrelated timeline work;
- selecting a clip cannot silently move transport;
- a frame-ready callback cannot claim that a frame was actually presented.

## Consequences

Positive consequences:

- playback workers never read live editor containers;
- all stale asynchronous results follow one testable rejection rule;
- old snapshots remain safely readable until bounded work releases them;
- editing preview and transport preview coexist without two transport
  authorities;
- frame pixels and their metadata reach the UI atomically;
- future CPU, D3D11, or QRhi presentation uses the same core identities.

Costs and limitations:

- playback-affecting edits allocate a new snapshot;
- media descriptors must be complete enough for workers to operate without the
  library;
- high-rate seeks create high-rate generations and presentation request IDs;
- source and sequence snapshot builders need explicit validation errors;
- milestone 1 bounds video work aggressively and may sacrifice throughput for
  deterministic responsiveness;
- audio queue policy remains deferred to ADR-004.

## Migration strategy

1. Introduce snapshot, revision, work-identity, and presentation-request value
   types with pure tests.
2. Add an editor-thread snapshot builder over the existing single V1/A1 model.
3. Adapt `TimelinePlaybackResolver` to consume only a snapshot.
4. Add generation gates around the existing Qt video seek/frame callbacks.
5. Add `PreviewPresentationCoordinator` behind the current presentation
   adapter and preserve the existing UI.
6. Route playback-affecting edits to build and install a new snapshot.
7. Remove live `TimelineModel`/`MediaLibrary` reads from the new playback path.
8. Introduce bounded latest-wins decode/composition ports before moving work to
   explicit application-owned threads.

Each step is feature-flagged as one complete path. A frame request/result never
crosses between legacy request-ID ownership and the new generation gate.

## Alternatives considered

### Lock the mutable editor model while playback reads it

Rejected. UI edits would contend with media work, and references could still
escape the lock into asynchronous callbacks.

### Deep-copy editor state inside every worker request

Rejected. It repeats validation and copying, loses one authoritative revision,
and makes two requests from the same edit difficult to correlate.

### Cancel every worker synchronously on seek or edit

Rejected. Decoder cancellation latency is implementation-specific and would
make UI responsiveness part of the correctness contract.

### Use PlaybackGeneration for editing-preview selection too

Rejected. Clip selection is not transport intent. Incrementing playback
generation for selection would reintroduce the coupling removed by ADR-002.

### Publish frame pixels and metadata separately

Rejected. The UI could pair a new position label with an old picture or accept
one half of a stale result.

## Acceptance criteria

ADR-003 is implemented when:

1. Snapshot construction validates one completed editor state and either
   returns one immutable snapshot or one error.
2. Snapshot code contains no Qt/MFC objects and no references to mutable editor
   containers.
3. Undo/redo and every playback-affecting edit produce a strictly newer
   revision; view-only changes do not.
4. Playback resolves sequence frames using only the accepted snapshot.
5. Every asynchronous request/result carries the required session, generation,
   source/sequence, revision, and media/clip identity.
6. Tests prove stale results cannot transition playback, update the renderer,
   publish an error, or clear the retained frame.
7. Repeated seeks allocate distinct generations and retain O(1) pending video
   work.
8. Snapshot replacement preserves or clamps position and preserves ADR-002's
   pending transport intent.
9. Selecting a clip while paused changes only presentation request identity;
   playhead, playback generation, and phase remain unchanged.
10. Resuming transport supersedes an editing-preview frame request.
11. Frame pixels and position metadata are accepted and published atomically.
12. `FramePresented` is distinguishable from decode/composition readiness and
    cannot advance transport.
13. Empty snapshots, unavailable media descriptors, missing descriptor
    references, duplicate results, out-of-order snapshot installs, viewport
    clear, and shutdown have deterministic tests.
14. Existing Qt widget tests pass unmodified through the feature-flagged
    presentation adapter.

## Learning focus

Immutability prevents readers from observing half of an edit. Revision tells us
which editor transaction produced a snapshot. Playback generation tells us
which asynchronous transport epoch is current. Presentation request ID tells us
which valid frame the viewport wants now. They solve different races and should
not be collapsed into one counter.
