# Milestone 3 — Playback authority

**Goal:** give the framework-neutral core a single, testable playback-state
authority — `PlaybackSession`, an injected clock/anchor, the ADR-002 engine
`PlaybackCommand` set, and the identity rules that reject stale asynchronous
results — entirely with fake ports. No engine thread, decoder, audio device,
UI notification queue, or MFC/Qt adapter exists yet; those belong to
Milestone 4. Nothing in this milestone changes what the current application
displays or how it plays media, because no UI path is routed through the new
session yet.

## Completed issues

1. **M3-01 — Injected playback clock and anchor resolution**
   - `IPlaybackClock` and `PlaybackAnchor` (ADR-004), plus a named function
     resolving sequence position from an anchor and a clock reading, built
     only from the existing `MediaTime.h` rate-conversion helpers.
   - A deterministic fake clock proves the equation at 24, 25, 30, and
     30000/1001 fps.
2. **M3-02 — PlaybackSession command/state machine**
   - The ADR-002 value types and the full milestone-1 command-policy table
     (`OpenSource`/`InstallSnapshot`/`Play`/`Pause`/`Stop`/`Seek`/`SetRate`/
     `Shutdown`), applied synchronously — with no decoder to wait for,
     Seeking/Prerolling resolve to their destination phase within the same
     call, the same "completes without a decoder" case ADR-002/ADR-004
     already describe for a still image or gap.
   - Adds `MediaAssetId`, the one type ADR-002 names but never defines, as a
     thin wrapper over the existing plain-int media-asset-id space.
   - A session is constructed for one `PlaybackSource` kind and keeps it for
     its lifetime this milestone; a command naming the other kind is
     rejected as `SourceKindMismatch` rather than silently accepted.
3. **M3-03 — Stale-result rejection and idempotent observations**
   - `PlaybackSession::isCurrent()` coverage across every ADR-002-listed
     generation-advancing trigger (seek, repeated scrubbing, source
     replacement, snapshot replacement, stop, shutdown).
   - `reportSourcePosition()`, a second synthetic observation beyond
     `reportFailure()`, proving the stale/idempotent pattern generalizes.
   - `PlaybackStatusGate`, the consumer-side "accept only a newer statusSeq
     within a session, reset on a new session" rule.

## What remains deliberately deferred

No engine thread, command queue, decoder, audio device, compositor, UI
notification bridge, or feature flag exists after this milestone. Nothing
routes through `PlaybackSession` yet — the current application's playback
behavior is unchanged. Those integrations are Milestone 4; making the new
path the default and retiring MFC timer advancement is Milestone 5.

## Verification

The framework-neutral core tests (`MiniEditorPlaybackCoreTests`), the
existing editor/Qt-widget regression tests (`MiniEditorCoreTests`,
`MiniEditorQtWidgetTests`), and the full application all build and pass in
both the Qt-enabled (`vs2022-x64`) and MFC-only (`vs2022-mfc-x64`)
configurations.

*Architecture: ADR-002, ADR-004 (partial), ADR-006/ADR-007 (reused
unmodified).*

## Original issue breakdown

The dependency graph and per-issue scope below are kept as the planning
record; see "Completed issues" above for what actually shipped.

```text
M3-01  Injected playback clock and anchor resolution
  |
  +-- M3-02  PlaybackSession command/state machine
        |
        +-- M3-03  Stale-result rejection and idempotent observations
```

Each issue only needs the ones above it; `main` stays buildable and
test-green after every issue.

## M3-01 — Injected playback clock and anchor resolution

Introduces the ADR-004 clock port and the pure position-derivation math,
without any audio/video scheduling (that is Milestone 4).

**Scope**

- `IPlaybackClock` (`virtual MasterClockTime now() const`), added to
  `MiniEditorPlaybackCore`.
- `PlaybackAnchor { MasterClockTime masterClock; SequenceTime sequenceTime;
  int playbackRatePercent; }`.
- A named anchor-resolution function implementing ADR-004's equation
  (`elapsedClock` → `elapsedSequence` via the existing `sequenceElapsedFor`
  from `MediaTime.h` → `sequenceTime` → `timelineFrame` via
  `frameAtSequenceTime`), built only from already-existing `MediaTime.h`
  helpers.
- A deterministic fake clock for tests (test-only code, not part of the
  public core library — production's `steady_clock` adapter is application
  layer per ADR-007 and is out of scope here).

**Non-goals**

- No re-anchoring *trigger* rules (those belong to the session's phase
  transitions in M3-02); this issue only builds the equation and the port.
- No audio/video underflow, decode queueing, or A/V-master selection
  (ADR-004's audio/video policy — Milestone 4).

**Done when**

- position is derived only from `clock.now()` and an anchor, never from a
  counter or accumulated rounded frame durations;
- the fake clock proves deterministic conversion at 24, 25, 30, and
  30000/1001 fps through the existing ADR-001 helpers;
- `MiniEditorPlaybackCoreTests` covers the anchor equation directly.

Architecture: ADR-004 (partial — acceptance criteria 1 and 3 only).

## M3-02 — PlaybackSession command/state machine

The center of this milestone: the ADR-002 value types and the complete
milestone-1 command policy table, executed synchronously (an `applyCommand`
call, not a background thread — the engine thread itself is ADR-005/
Milestone 4 scope).

**Scope**

- ADR-002's value types: `PlaybackSource` (`SourceAssetPreview`,
  `SequencePreview`), `SourceCompletionPolicy`, `PlaybackPhase`,
  `SourcePreviewStatus`, `SequencePreviewStatus`, `PlaybackContext`,
  `PlaybackStatus`, `PlaybackCommandRejected`, `PlaybackRejectReason`,
  `PlaybackCommandId`, `PlaybackSessionId`, `PlaybackGeneration`,
  `StatusSequenceNumber`.
- The 8-alternative `PlaybackCommand` variant (`OpenSource`,
  `InstallSnapshot`, `Play`, `Pause`, `Stop`, `Seek`, `SetRate`, `Shutdown`)
  with the payload each needs (for example `OpenSource` carries the source
  identity and its completion policy; `InstallSnapshot` carries a
  `SequencePlaybackSnapshotPtr`).
- `PlaybackSession`, applying every row of ADR-002's command-policy table
  (`OpenSource`/`InstallSnapshot`/`Play`/`Pause`/`Stop`/`Seek`/`SetRate`/
  `Shutdown` from every documented phase, including the `Failed`-recovery
  and post-shutdown rejection rules) and publishing `PlaybackStatus`.
- Wires M3-01's clock/anchor into `SequencePreviewStatus` position
  derivation; `SourceAssetPreview` continues to only adopt position from
  observations, exactly as ADR-002/ADR-004 specify for milestone 1.
- `sourceProgressPermille()`/`sourceTimeZero()` named conversions.

**Design note requiring a small new type, not a new rule:** ADR-002's
`SourcePreviewStatus` types its source identity as `MediaAssetId`, but no
such type exists yet — the codebase uses a plain `int` for media-asset ids
everywhere (`PlaybackMediaDescriptor::mediaAssetId`,
`EditorSession::timelineClipboardMediaAssetId()`). This issue introduces
`MediaAssetId` as a thin explicit wrapper over that same existing int space
(constructible from `int`, not a fresh runtime-generated identity like
`ProjectId`/`SequenceId`), used only at this new command/status boundary. It
does not change `PlaybackMediaDescriptor`'s existing field or any editor-side
code. This is called out here because it is the one place this issue adds a
symbol an ADR names but never defines — flagging it instead of leaving it
implicit.

**Non-goals**

- No engine thread, command queue, or cross-thread delivery (ADR-005 —
  Milestone 4).
- No decoder/audio/compositor observations — this issue proves the state
  machine with commands and clock readings only.
- No UI submission path or feature flag (Milestone 5).

**Done when**

- pause/seek/resume/stop/completion/failure transitions have direct
  state-machine tests covering every phase row in ADR-002's table;
- an invalid command in a given phase produces `PlaybackCommandRejected`
  carrying its `PlaybackCommandId` and a reason;
- from `Failed`, only `Stop`/`OpenSource`/`InstallSnapshot`/`Shutdown` are
  accepted, and all commands are rejected once shutdown completes;
- `PlaybackContext`'s active alternative always matches the active
  `PlaybackSource`, and `error` is set if and only if phase is `Failed`;
- source-progress tests cover a zero end time and clamping below 0 and above
  1000.

Architecture: ADR-002 (state-machine and value-type criteria only — 1, 2, 4,
7, 10, 11, 14, 15; the rest depend on real backends/UI and are deferred),
ADR-004 (anchor wiring only), ADR-006/ADR-007 (reuses existing identity and
snapshot types without modification).

## M3-03 — Stale-result rejection and idempotent observations

Generalizes the session/generation identity check so a later real backend
(Milestone 4) has a single, already-tested rule to call instead of
reinventing currency checks per worker.

**Scope**

- A named currency check (for example
  `PlaybackSession::isCurrent(PlaybackSessionId, PlaybackGeneration) const`)
  that later observation/result handling can call before applying any
  effect.
- Generation-advance coverage for every ADR-002-listed trigger: accepted
  seek (including repeated scrubbing), source replacement, snapshot
  replacement, and stop/shutdown discarding pending work.
- `statusSeq` monotonicity within one `PlaybackSessionId`, resetting for a
  new session; a consumer-side helper that accepts only a newer `statusSeq`.
- Tests built from synthetic (not real-decoder) observations proving: a
  stale session/generation is discarded without a transition; a duplicate
  or non-advancing observation is idempotent; an out-of-order `statusSeq`
  publication is discarded by a consumer.

**Non-goals**

- No real decoder/audio/compositor worker or the queues ADR-005 defines —
  those consume this rule in Milestone 4, they are not built here.
- No proof that legacy `EditorSession` mutators are never called — that
  needs the feature-flagged UI path from Milestone 5.

**Done when**

- a stale observation cannot change session state after a source or
  generation replacement;
- duplicate observations are idempotent;
- every ADR-002-listed generation-advancing trigger is covered by a test.

Architecture: ADR-002 (criteria 5, 8, 12), ADR-003 (forward-compatible
identity shape only — no snapshot/generation mechanics change).

## What remains deliberately deferred

No engine thread, command queue, decoder, audio device, compositor, UI
notification bridge, or feature flag exists after this milestone. Nothing
routes through `PlaybackSession` yet — the current application's playback
behavior is unchanged. Those integrations are Milestone 4; making the new
path the default and retiring MFC timer advancement is Milestone 5.

## Human decision gates

No decision is needed to start M3-01 or M3-02: both directly apply accepted
ADR-002/ADR-004 text, and M3-02's one new type (`MediaAssetId`) is a minimal
wrapper flagged above rather than a new authority or command rule. M3-03 is
a generalization of already-accepted ADR-002 identity rules. A new product
requirement, an additional transport command, or a change to the accepted
command/status variants pauses the automation and requires a human decision.

## Agent handoff rule

An implementation agent may take exactly one ready issue. It reports the
changed files, build/test commands, and any decision gate it encountered.
The next ready issue starts only after its predecessor is reviewed and
merged, so `main` remains a known-good learning baseline.
