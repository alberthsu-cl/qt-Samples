# ADR-002 Review 1 — PlaybackSession Is the Playback-State Authority

Verdict: **Accept with revisions**

Round: 1 (original review)

Primary document:
[`../decisions/0002-playback-session-is-the-state-authority.md`](../decisions/0002-playback-session-is-the-state-authority.md)
(Status: Proposed)

Reviewed against
[ADR-001](../decisions/0001-strong-media-time-domains.md) (Accepted, must stay
closed), [`../target-playback-architecture.md`](../target-playback-architecture.md),
[`../current-playback-architecture.md`](../current-playback-architecture.md),
and the current migration code: `EditorSession`, `PlaybackBackend`,
`QtMediaPlaybackBackend`, `PlaybackClockController`, `EditorCommandController`,
`PreviewStateResolver`, `MainFrame`.

## Verdict

The central decision is correct and should survive review: one mutable
authority for transport state, an explicitly named playback source, commands in
and immutable status out, and observations that cannot write authority. The
four rejected alternatives are each rejected for the right reason.

It is not yet implementation-ready. ADR-002's job is to make playback a
deterministic state machine, and the state machine is incomplete: `Failed` has
no exit, six phase/command pairs are undefined, and `PlaybackStatus` cannot
drive the UI it is supposed to replace. Two contracts that later ADRs must
attach to — the clock boundary and the command/acknowledgement shape — are
missing rather than deferred. And the migration as written permits two
authorities to coexist rather than making the flag decide ownership.

None of the revisions expand scope. Most narrow it, by writing down decisions
that are currently implied.

## Blocking findings

### B1 — PlaybackStatus cannot populate the cache the migration promises

*Sections: Decision; Migration strategy.*

The migration says the existing `PlaybackState` "may remain as a read-only UI
presentation cache populated from `PlaybackStatus`". It cannot be, as specified.
`PlaybackState` carries `durationFrames`, and today the transport slider's range
comes from it. `PlaybackStatus` has no duration field, so the cache has no
source for a value the UI already renders.

Two further omissions:

- **No error payload.** State-transition ownership says "Failure produces
  `Failed` with an error value", but the struct has no such member. The error
  value exists in prose only.
- **No way to distinguish the authoritative position from the displayed frame.**
  Pause "freezes the authoritative position and retains the displayed frame".
  During `Seeking` and `Prerolling` those differ, and the UI needs both to avoid
  flicker.

See section 5 for a minimal replacement.

### B2 — TimelineFrame for SourceAssetPreview silently invents a sequence grid

*Sections: Decision.*

`PlaybackStatus::timelineFrame` is a `TimelineFrame` regardless of which
`PlaybackSource` alternative is active. ADR-001 defines `TimelineFrame` as a
"nonnegative position on the sequence grid" and states that "Every in-memory
`TimelineSequence` carries a `FrameRate`".

A source-asset preview has no sequence and therefore no sequence rate. Reporting
its position as a `TimelineFrame` either invents an implicit 30/1 grid or
reinterprets source time on a grid that does not exist — precisely the domain
confusion ADR-001 was accepted to prevent. This is also how the *current* code
misbehaves: `QtMediaPlaybackBackend::unclampedPlayerPositionFrame()` converts
`QMediaPlayer::position()` into frames at the session's fixed 30 fps.

ADR-001 is closed and must not be reopened, so the fix belongs here: make the
reported position a variant whose alternative matches the source alternative.

### B3 — The clock boundary is unstated, so ADR-004 has nothing to attach to

*Sections: Decision ("The session owns … master-clock anchor").*

ADR-002 never mentions `IPlaybackClock`, although ADR-001 defines
`MasterClockTime` as "instant measured by `IPlaybackClock`". The session is said
to own the anchor, but nothing says the session **reads** time from an injected
clock port rather than measuring it.

That single sentence is the contract ADR-004 needs. Without it, ADR-004 cannot
substitute an audio-derived clock without renegotiating ownership, and nothing
prevents an implementer calling `steady_clock::now()` inside the session — which
would also make the pure state-machine tests in acceptance criterion 7
non-deterministic.

The anchor's representation is also unstated. ADR-001 already supplies the
types: the anchor is a `(MasterClockTime, SequenceTime)` pair. Naming it costs
one line and stops ADR-004 inventing a different one.

### B4 — The temporary façade is synchronous; the commands are not

*Sections: Migration strategy ("The current `IPlaybackBackend` remains a
temporary façade").*

`IPlaybackBackend::executeCommand()` returns `PlaybackClockAction` **by value**,
and `MainFrame::applyPlaybackClockAction()` acts on that return immediately to
start or stop the MFC timer. `seek()`, `synchronize()` and `advanceOneFrame()`
do the same.

Under ADR-002 a command is queued and applied on the serialized engine context,
so the resulting phase is not known when the façade returns. The façade
therefore has three options, and the ADR picks none: block on the engine thread
(which target Decision 7 forbids the UI from doing), return a guess, or change
meaning.

The workable answer is the third: the return value becomes "the command was
accepted", and the timer policy is driven by published `PlaybackStatus` rather
than by a command's return. Say so, or the first implementer will block the UI
thread.

### B5 — The command set does not cover an existing editor-to-transport interaction

*Sections: Decision (`PlaybackCommand`); Separation from EditorSession.*

`TimelineEditingController` calls
`EditorSession::leavePausedTimelinePlaybackForEditing()`, which clears
`isPaused` when the user selects an edit target — an **edit gesture mutating
transport state**.

ADR-002's separation table assigns "running/paused/stopped state" to
`PlaybackSession`, and the Decision section says "An editor action may cause a
playback command". But no command in the variant expresses this one. Milestone 1
must either enumerate it (a `LeavePaused` or `ExitPausedForEdit` command) or
state that the behaviour is dropped. As written, an implementer will reach
around the authority to preserve current behaviour.

Related: the ADR says source-asset preview keeps its "hold the last frame"
completion policy, "selected by an explicit source policy". No such policy value
appears in `PlaybackSource`, `OpenSource`, or `PlaybackStatus`. Name it.

### B6 — `Failed` has no defined exit

*Sections: Decision (`PlaybackPhase`); State-transition ownership.*

`Failed` is listed as a phase and produced on failure, but nothing says how a
session leaves it. Which commands are legal in `Failed`? Does `Stop` clear it?
Does `OpenSource` on a different asset recover, or must the session be
recreated? Is `Failed` sticky across a new snapshot?

A terminal-looking phase with undefined exits is the one part of a state machine
guaranteed to be implemented differently by every reader. Recovery is in scope
for ADR-002; only the *cause* of failure belongs to decoder ADRs.

### B7 — Six phase/command pairs are undefined

*Sections: State-transition ownership.*

The bulleted behaviours cover the common path but leave the rest to inference.
The full grid is in section 4; the undefined cells are `Seek` while `Seeking`,
`Seek` while `Prerolling`, `Play` while `Prerolling`, `SetRate` while `Seeking`
or `Prerolling`, `InstallSnapshot` while not `Stopped`, and `Shutdown` from
every phase.

`Seek` during `Seeking` is the one that matters most: scrubbing a slider
produces exactly that, repeatedly, and the answer determines whether generation
advances per seek or per settled seek.

### B8 — The migration permits two authorities, and the flag's meaning is unstated

*Sections: Migration strategy.*

Step 6 defers narrowing the legacy mutation APIs until "after source preview
also migrates". During milestone 1, therefore, `EditorSession` still exposes
seven playback mutators:

```text
handlePlaybackCommand()            seekTimeline()
advancePlaybackFrame()             setPlaybackDuration()
updatePlaybackFromBackend()        updatePlaybackRatePercent()
leavePausedTimelinePlaybackForEditing()
```

`updatePlaybackFromBackend()` is the sharp one: `QtMediaPlaybackBackend` calls
it from five sites to write position, duration and playing/paused straight into
`EditorSession`. That is a media backend publishing an authoritative position —
the exact thing the *Player and decoder callbacks are observations* section
forbids.

ADR-002 says the legacy state "must not remain an independent authority" but
does not say what enforces that. The compile-time flag must be stated to make
ownership **mutually exclusive** — on the new path, the legacy mutators are not
called at all, ideally not compiled — rather than leaving two writable states
that happen to agree. Two mutable states that are kept in sync is the very
condition ADR-002 exists to end.

## Non-blocking improvements

**N1 — Presentation resolvers still read editor focus.**
`PreviewStateResolver::resolve()` reads both `session.playbackState()` and
`session.isTimelineFocused()`. Under ADR-002 the preview mode should follow
`PlaybackStatus::source`, not editor focus. The six migration steps do not
mention presentation resolvers; add one.

**N2 — Rate persistence across source replacement is undefined.**
`EditorSession::updatePlaybackRatePercent()` deliberately writes both source and
timeline states so "preview speed is one transport preference". After
`OpenSource`, does `ratePercent` persist or reset to 100? Say which.

**N3 — The heartbeat should not depend on rate.**
`PlaybackClockController::tickIntervalMillisecondsForRate()` derives the timer
interval from playback rate, which made sense when the timer advanced frames.
A presentation heartbeat samples status and should tick at a constant,
rate-independent interval.

**N4 — Two referenced files do not exist.**
`IPlaybackBackend` and `SimulatedPlaybackBackend` both live in
`PlaybackBackend.h` / `.cpp`. Issues or ADR text citing `IPlaybackBackend.*` or
`SimulatedPlaybackBackend.*` as paths will not resolve.

**N5 — Per-request correlation already exists in the codebase.**
`PreviewSeekRequest::requestId` and `PreviewSeekResult::requestId` already
implement request/result correlation, with the comment "A backend may receive
completions out of order and must accept only the result belonging to its newest
request." ADR-002 relies on `PlaybackGeneration` alone. Cite the existing
pattern; it is the precedent for the command IDs recommended in section 6.

**N6 — Acceptance criterion 9 is subjective.** "Without visible redesign" has no
pass condition. Bind it to something checkable, such as the existing Qt widget
tests continuing to pass unmodified.

**N7 — Extend the Learning focus.** The ownership/authority distinction is well
put. Add the third one this ADR needs: *measurement* — the clock measures, the
session decides. That is the B3 boundary in one sentence.

## 4. State and command transition table for milestone 1

Rows are the current phase; cells give the resulting phase. **UNDEFINED** marks
what the ADR does not currently answer.

| | Stopped | Seeking | Prerolling | Playing | Paused | Failed |
| --- | --- | --- | --- | --- | --- | --- |
| **OpenSource** | Stopped (gen++) | Stopped (gen++) | Stopped (gen++) | Stopped (gen++) | Stopped (gen++) | **UNDEFINED** (B6) |
| **InstallSnapshot** | Stopped (gen++) | **UNDEFINED** | **UNDEFINED** | **UNDEFINED** — target Decision 4 says seek/preroll; phase outcome unstated | **UNDEFINED** | **UNDEFINED** (B6) |
| **Play** | Prerolling → Playing | **UNDEFINED** | **UNDEFINED** (already prerolling) | Playing (no-op) | Prerolling → Playing | **UNDEFINED** (B6) |
| **Pause** | Stopped (no-op) | **UNDEFINED** | Paused — stated, but preroll completion policy unstated | Paused | Paused (no-op) | **UNDEFINED** (B6) |
| **Stop** | Stopped (no-op) | Stopped (gen++) | Stopped (gen++) | Stopped at start (gen++) | Stopped at start (gen++) | **UNDEFINED** (B6) |
| **Seek** | Seeking → Paused | **UNDEFINED** (B7) — scrubbing | **UNDEFINED** (B7) | Seeking → Prerolling → Playing | Seeking → Paused | **UNDEFINED** (B6) |
| **SetRate** | **UNDEFINED** (store only?) | **UNDEFINED** | **UNDEFINED** | Playing, re-anchored | Paused | **UNDEFINED** (B6) |
| **Shutdown** | **UNDEFINED** | **UNDEFINED** | **UNDEFINED** | **UNDEFINED** | **UNDEFINED** | **UNDEFINED** |

Observations, which never change authority by themselves:

| Observation | Accepted when | Effect |
| --- | --- | --- |
| SourceLoaded | session + generation match | Prerolling → Playing or Paused per pending intent |
| FrameReady | session + generation match | may satisfy preroll; otherwise queued |
| EndOfSource | session + generation match, phase Playing | **Sequence:** Stopped at start. **Source asset:** hold last frame — policy value not yet named (B5) |
| MediaFailed | session + generation match | Failed + error value (B1, B6) |
| ClockObservation | always | no phase change; anchor arithmetic only |
| *any of the above, stale* | generation or session mismatch | **discarded, no transition** |

Two notes on determinism (question 9). Discard-on-mismatch makes *late*
observations safe, and that is well covered. **Duplicate** observations are not:
two `EndOfSource` events with a matching generation would both be accepted, and
the second arrives when the phase is already `Stopped`. The rule needs to be
"an observation that does not change the phase is idempotent", stated once.

`Shutdown` is undefined from every phase and is the highest-value row to fill:
it is the only command whose contract other ADRs (thread join, worker
cancellation) depend on.

## 5. Recommended minimal PlaybackStatus

Satisfies both source alternatives without touching ADR-001:

```cpp
struct SourcePreviewPosition {          // SourceAssetPreview
    SourceTimestamp sourceTime;
    SourceTimestamp sourceDuration;
};

struct SequencePreviewPosition {        // SequencePreview
    TimelineFrame timelineFrame;
    FrameCount    sequenceDuration;
    FrameRate     frameRate;
};

using PlaybackPosition = std::variant<
    SourcePreviewPosition,
    SequencePreviewPosition>;

struct PlaybackStatus {
    PlaybackSessionId    sessionId;
    PlaybackGeneration   generation;
    StatusSequenceNumber statusSeq;      // monotonic per session
    PlaybackSource       source;
    PlaybackPhase        phase;
    PlaybackPosition     position;       // alternative matches `source`
    int                  ratePercent = 100;
    std::optional<PlaybackError> error;  // engaged if and only if phase == Failed
    PlaybackCommandId    lastAppliedCommandId;
};
```

Why each addition earns its place:

- **`PlaybackPosition` variant** resolves B2. Source preview reports
  `SourceTimestamp`, its natural domain, with no invented grid. Sequence preview
  reports `TimelineFrame` plus the `FrameRate` the UI needs to format timecode
  under ADR-001's rule that "Timecode is formatted from `TimelineFrame`".
- **Duration alongside position**, per alternative, resolves the slider-range
  half of B1.
- **`error` as `optional`** makes "engaged if and only if `Failed`" a testable
  invariant rather than prose.
- **`statusSeq`** lets the UI discard out-of-order publications without
  understanding phases — see section 6.
- **`lastAppliedCommandId`** gives acknowledgement without a second channel.

One invariant worth stating explicitly, because it is cheap to test: the
`position` alternative index always equals the `source` alternative index.

The displayed-frame half of B1 is deliberately **not** solved here. A
`presentedFrame` field belongs with frame publication in ADR-003; ADR-002 need
only say that `position` is the authoritative transport position and is not
required to equal the frame currently on screen.

## 6. Command, observation and publication contract

**Commands** are immutable values submitted to one queue and applied in
submission order on the engine context. Each carries:

```cpp
struct PlaybackCommandEnvelope {
    PlaybackCommandId id;          // monotonic, engine-assigned on accept
    PlaybackSessionId targetSession;
    PlaybackCommand   command;
};
```

**Are command IDs necessary? Yes — but acknowledgement should not need a second
channel.** Two problems make bare fire-and-forget insufficient:

1. A UI that submits `Play` cannot otherwise distinguish "not applied yet" from
   "applied and immediately superseded", so a transport button cannot settle.
2. A command invalid in the current phase currently has no way to report that.
   With B6 unresolved, `Play` in `Failed` would fail silently.

The cheap resolution is **acknowledgement by status**: every publication carries
`lastAppliedCommandId`, so the UI knows its command landed by observing status
rather than by awaiting a reply. Add one event for the negative case:

```cpp
CommandRejected { PlaybackCommandId id; PlaybackRejectReason reason; };
```

That is one field and one event — materially less machinery than a
request/response protocol, and it removes the silent-failure mode.

**Observations** carry `PlaybackSessionId` and `PlaybackGeneration` and are
accepted only when both match. They never carry a position that becomes
authoritative; the session decides whether an observation causes a transition.
Observations that would not change the phase are idempotent.

**Publication** answers question 12. `PlaybackStatus` is constructed as a
complete value *after* a transition finishes and is then published; it is never
mutated in place and never published mid-transition. Because one engine context
performs all transitions, publication order equals transition order. `statusSeq`
increases by one per publication; a UI that receives a lower `statusSeq` than it
has already applied discards it. That makes partial or out-of-order state
unobservable without requiring the UI to reason about phases.

## 7. Exact recommended ADR edits

| # | Section | Edit | Ref |
| --- | --- | --- | --- |
| 1 | Decision | Replace the `PlaybackStatus` struct with section 5's, including the position variant, duration, optional error, `statusSeq` and `lastAppliedCommandId`. | B1, B2 |
| 2 | Decision | Add one sentence: the session **reads** time from an injected `IPlaybackClock` and never measures it; the anchor is a `(MasterClockTime, SequenceTime)` pair. | B3 |
| 3 | Decision (`PlaybackCommand`) | Add the editor-driven pause-exit command, and name the source completion policy carried by `OpenSource`. | B5 |
| 4 | Decision | State the position-alternative-matches-source invariant. | B2 |
| 5 | State-transition ownership | Replace the bullet list with the full grid from section 4; define `Failed` exits and `Shutdown` from every phase. | B6, B7 |
| 6 | Player and decoder callbacks are observations | Add the idempotence rule for non-transitioning observations. | B7 |
| 7 | Session identity and generation | State whether generation advances per `Seek` or per settled seek, since the scrubbing case depends on it. Detailed invalidation stays with ADR-003. | B7 |
| 8 | Migration strategy | State that the compile-time flag makes ownership mutually exclusive, that legacy mutators are not called on the new path, and that `updatePlaybackFromBackend()` becomes an observation rather than a write. | B8 |
| 9 | Migration strategy | State that the temporary `IPlaybackBackend` return value means "command accepted", and that timer policy follows published status. | B4 |
| 10 | Migration strategy | Add a step for presentation resolvers to consume `PlaybackStatus::source` instead of `isTimelineFocused()`. | N1 |
| 11 | Acceptance criteria | Add the four criteria in section 9. | — |
| 12 | Learning focus | Add measurement as the third distinction. | N7 |

## 8. Explicit deferrals

**ADR-003** — snapshot construction and content; what invalidates a generation
and how pending work is discarded; frame publication including any
`presentedFrame`; queue bounds. ADR-002 should reference generation only as an
identity that travels with commands, observations and publications.

**ADR-004** — which clock `IPlaybackClock` supplies; audio-derived clocking;
drift correction; re-anchoring policy on pause, seek and rate change. ADR-002
supplies only the injection boundary from B3.

**ADR-005** — engine-thread creation and join, Qt `QObject` affinity, adapter
thread rules. ADR-002 needs `Shutdown`'s *state-machine* contract (section 4's
missing row); the thread lifetime that implements it belongs to ADR-005.

Also out of scope and correctly so: decoder seek and keyframe selection (already
deferred by ADR-001), UI redesign, and multi-session concurrency. On question
16, explicit `PlaybackSessionId` on every command, observation and publication
is sufficient to keep multiple sessions possible later; milestone 1 should
create exactly one and say so.

## 9. Are the acceptance criteria sufficient and testable?

The nine criteria are objective and testable under MSVC C++17 with fake clocks
and fake media ports, with two exceptions: criterion 9 is subjective (N6), and
criterion 7 cannot be met deterministically until B3 injects the clock — a
session calling `steady_clock::now()` internally cannot have a pure
state-machine test.

Four gaps, matching the blocking findings:

1. **Status completeness.** Every published status has an `error` engaged if and
   only if `phase == Failed`, and a `position` alternative matching `source`.
2. **Failure recovery.** From `Failed`, the defined recovery commands reach a
   defined phase, and commands that are not legal are rejected rather than
   ignored.
3. **Command rejection.** A command invalid in the current phase produces
   `CommandRejected` carrying its `PlaybackCommandId`.
4. **Duplicate and out-of-order determinism.** Criterion 8 covers stale
   observations after replacement; add that a duplicate matching observation is
   idempotent, and that a status with a lower `statusSeq` is discarded by the UI
   adapter.

A fifth is worth adding for B8, and it is the criterion that most directly
proves the ADR's thesis: **with the new path enabled, no legacy
`EditorSession` playback mutator is called.** That is checkable with a test
double or an assertion build, and it is the difference between one authority and
two that agree.

## Reviewer's note

ADR-002's reasoning is sound and its alternatives analysis is the strongest part
of the document. The revisions above are almost entirely about writing down
decisions the ADR already implies — the state machine it promises, the clock
boundary it depends on, and the flag semantics its migration assumes. The one
place it genuinely under-reaches is `PlaybackStatus`, which is currently too
small to replace the state it is meant to replace, and which reintroduces an
ADR-001 domain confusion for source preview.
