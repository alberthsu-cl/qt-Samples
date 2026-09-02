# ADR-002 Review 3 — Final Acceptance Gate

Verdict: **Accept with minor editorial changes**

Round: 3 (final acceptance gate)

Revision reviewed: `9448227` *Revise ADR-002 playback authority contract*
(baseline `4b5408c`, +168 / −24, confined to ADR-002).

Reviewed against [ADR-001](../decisions/0001-strong-media-time-domains.md)
(Accepted, closed), [review 1](adr-002-review-1-playback-authority.md),
[review 2](adr-002-review-2-resolutions.md),
[`../current-playback-architecture.md`](../current-playback-architecture.md),
[`../target-playback-architecture.md`](../target-playback-architecture.md), and
the project `README.md`.

This is an acceptance gate, not a design pass. No alternative architecture is
proposed and no deferred ADR-003/004/005 work is reopened.

## Verdict

Every one of the eleven findings is closed. The revision adopts the resolutions
faithfully, and in three places it is more precise than the resolutions were:
`Stopped` and `Paused` are now *defined* rather than assumed, the Shutdown
acceptance gate is explicitly separated from `PlaybackPhase`, and the
source-preview position rule names validation before adoption.

Two defects remain, both found by pushing on the new text rather than by
re-litigating old findings. Each is one or two sentences to fix, neither
changes a decision, and both are listed as F1 and F2 below. They gate the status
flip because the ADR now asserts "The complete milestone-1 command policy is",
and a reader is entitled to take that literally.

## 1. Findings B1–B8 from review 1

| ID | Status | Evidence in the revision |
| --- | --- | --- |
| B1 | **Closed** | `PlaybackContext` carries `sourceEndTime` / `sequenceDuration`; `std::optional<PlaybackError> error`; presented frame "intentionally absent and is specified by ADR-003". `lastAppliedCommandId` is `std::optional`, which also answers the absent-before-any-command case. |
| B2 | **Closed** | `SourcePreviewStatus` reports `SourceTimestamp`. No implicit sequence grid anywhere in the status. |
| B3 | **Closed** | "The session receives an injected `IPlaybackClock` … Session code does not call `std::chrono::steady_clock` directly", the `(MasterClockTime, SequenceTime)` anchor for sequence playback, and the milestone-1 source rule. The review-2 gap is filled explicitly. |
| B4 | **Closed** | "Command submission never waits for media work", plus the UI-timers paragraph retiring the façade's timer directive by name. |
| B5 | **Closed** | Resolved structurally rather than by adding a command: "Selecting a timeline clip is not transport intent." No `ExitPausedForEdit` was introduced. |
| B6 | **Closed** | "From `Failed`, only `Stop`, `OpenSource`, `InstallSnapshot`, and `Shutdown` are accepted", with each recovery path spelled out in the table. |
| B7 | **Closed** | All four cells review 2 left open are now answered — see section 2. |
| B8 | **Closed** | The flag "chooses exactly one mutable authority", legacy mutators uncalled, read-only cache, display-only lossy conversion, `PreviewStateResolver` fed from status. |

## 2. Findings B9–B11 and the partial B3 / B7 from review 2

| ID | Status | Evidence |
| --- | --- | --- |
| B9 | **Closed** | `sourceProgressPermille()` is declared as a named boundary conversion, clamped to `[0, 1000]`, zero when `end` is source-time zero. Acceptance criterion 15 tests it. |
| B10 | **Closed** | The playhead, phase and command set are all stated to be unchanged by clip selection, and the idle-editing-preview nuance is preserved rather than lost. Criterion 14 tests it. |
| B11 | **Closed** | "source preview with `HoldLastFrame` finishes in `Paused` at `sourceEndTime`; sequence preview with `ReturnToStart` finishes in `Stopped` at timeline frame zero", with `Stopped` now explicitly defined as "positioned at its defined start". |
| B3 (partial) | **Closed** | "For milestone-1 source preview, authoritative `SourceTimestamp` progress is adopted from backend observations only after the session validates their session and generation." This is the observation route review 2 recommended, and it keeps ADR-004's unification open. |
| B7 (partial) | **Closed** | `InstallSnapshot` during `Seeking`/`Prerolling` is defined; the `HoldLastFrame` phase is defined; Shutdown's phase behaviour is defined; and `Paused` is defined as freezing transport "without claiming that a renderer has already presented the requested frame". |

The B7 answer on `Paused` deserves a note. Review 2 asked only whether `Paused`
implies a presented frame. The revision answers that and draws the boundary in
the same sentence by handing renderer acknowledgement to ADR-003 — which
prevents the question resurfacing there as an assumption.

## 3. Does PlaybackContext make mismatched domains unrepresentable?

**Yes, within a status value, and the boundary hazard review 2 raised is now
closed too.**

A `SequenceId` cannot be paired with a `SourceTimestamp` because they occupy
different alternatives of one variant. The revision also adds the sentence
review 2 asked for:

> `PlaybackContext` is derived from the active `PlaybackSource` plus resolved
> media metadata. Their alternatives correspond one-to-one …

That converts two independently maintained variants into one derived from the
other, so a future third source kind cannot be added to only one side without
the derivation failing to compile or the correspondence test failing.

`phase` and `error` remain independently variable, as review 2 accepted. The
iff-invariant is prose backed by acceptance criterion 10, which is the right
trade for milestone 1.

## 4. Is SourceTimestamp arithmetic confined, and is ADR-001 reopened?

**Confined, and ADR-001 is not reopened.**

The ADR adds no operator to `SourceTimestamp`. It declares one named function
and states that its implementation "may access framework units inside the
UI/media adapter, but raw source-time arithmetic does not escape that
boundary". ADR-001 already permits raw access from "named conversion helpers,
serialization code, and framework adapters", so the helper sits inside an
allowance that already exists rather than widening it.

The comparison implied by "zero when `end` is source-time zero" is legal:
ADR-001's F1 revision grants relational comparison between two values of the
same type, `SourceTimestamp` included.

One nit inherited from ADR-001 rather than introduced here: `SourceTimestamp`
has no `zero()` among ADR-001's five origin constants, so "source-time zero"
must be constructed inside a named helper. `sourceProgressPermille()` is such a
helper, so nothing is blocked. Noted as N1 only so it is not rediscovered.

## 5. The clock distinction

All three required properties hold.

- **Sequence position** uses the injected clock and the
  `(MasterClockTime, SequenceTime)` anchor.
- **Milestone-1 source position** is adopted from backend observations "only
  after the session validates their session and generation", and the
  observations section repeats the rule from the other side: a
  `positionChanged` callback "may report a candidate `SourceTimestamp`; after
  identity validation, the session may adopt it". *May adopt* keeps the session
  deciding, so this is adoption, not a backend writing authority.
- **ADR-004** retains advancement, rate conversion and re-anchoring, and is
  explicitly permitted to unify the two later "without changing this ownership
  boundary".

The asymmetry between the two contexts is deliberate and stated, which is what
review 2 asked for. It is also honest about the current stack: `QMediaPlayer`
self-clocks source playback today and nothing in milestone 1 changes that.

## 6. Transport versus editing presentation

Every required property is present, in one paragraph:

> Selecting a timeline clip is not transport intent. It changes the editor's
> presentation target only: the playhead and playback phase remain unchanged and
> no playback command is submitted.

Idle editing preview may still use selection; active transport context comes
only from `PlaybackStatus`; the non-playhead frame request is ADR-003's.
Selecting a *source asset* remains different and is correctly separated, because
it genuinely is transport intent.

This also restores agreement with the product README, which records that
"Selecting a clip no longer resets the head, so the same ruler position can
immediately enable Split" — the behaviour review 2's B10 was protecting. The
revision no longer conflicts with acceptance criterion 9.

## 7 and 8. Command rules per phase

The table covers `Stopped`, `Seeking`, `Prerolling`, `Playing`, `Paused`,
`Failed` and post-shutdown for all eight commands. Checking the eight items
called out for particular attention:

| Item | Result |
| --- | --- |
| Every accepted Seek increments generation | **Present**, including "each repeated scrubbing request" in the generation list and "immediately increment generation" in the table. |
| Repeated scrubbing supersedes old work | **Present** — "A newer seek supersedes older work." |
| `InstallSnapshot` during Seeking/Prerolling | **Present** — replace in-flight request, increment generation, preserve pending Play/Pause intent. |
| Failed recovery | **Present** and exhaustive. |
| Source `HoldLastFrame` → `Paused` at `sourceEndTime` | **Present.** |
| Sequence `ReturnToStart` → `Stopped` at frame zero | **Present.** |
| Shutdown preserves final phase, closes acceptance separately | **Present** — "one final status whose playback phase is unchanged … The acceptance gate is lifecycle state separate from `PlaybackPhase`." |
| `Paused` freezes transport without guaranteeing presentation | **Present.** |

Two gaps remain in the table; both are new findings, F1 and F2 below.

## 9. The asynchronous contract

| Requirement | Result |
| --- | --- |
| Submission never waits for media work | **Present** |
| Queue closure can reject immediately | **Present** — "Queue-closure rejection may be returned immediately" |
| Phase-dependent rejection carries command ID and reason | **Stated, but has no channel** — F1 |
| Accepted commands acknowledged in published status | **Present** via `lastAppliedCommandId` |
| `lastAppliedCommandId` may be absent | **Present** — it is `std::optional` |
| `statusSeq` monotonic per session and reset for a new session | **Present** — "increases monotonically within one `PlaybackSessionId` and restarts for a new session. … A new session ID resets that comparison." |

## 10. Migration ownership

All five required properties are present and unambiguous: the flag chooses
exactly one mutable authority; legacy mutators are not called on the new path;
`PlaybackState` is a read-only presentation cache never written back; any
source-time-to-frame value in it is "explicitly display-only and lossy" and
never engine input; and `IPlaybackBackend::executeCommand()` "no longer returns
a timer directive and `MainFrame::applyPlaybackClockAction()` is not used".

Naming `applyPlaybackClockAction()` specifically is what makes this checkable
against the code rather than aspirational.

## Newly discovered blockers

### F1 — Rejection is required by an acceptance criterion but has no channel

*Sections: Decision (command submission paragraph); Acceptance criteria 11.*

The ADR states:

> phase-dependent rejection is published by the engine with the command ID and
> a reason.

No type is declared for it. `PlaybackStatus` has no rejection member, and the
only publication the ADR describes is `PlaybackStatus`. The target document's
event set is `StatusChanged`, `VideoFrameReady`, `PlaybackEnded`, `MediaFailed`
— it has no rejection alternative either. Acceptance criterion 11 therefore
requires an observable that nothing produces.

This is reachable on the first Play from `Failed`, and silent rejection is the
exact failure mode review 1's B4 set out to remove.

**Minimal correction.** Declare the value alongside `PlaybackStatus` in the
Decision section:

```cpp
struct PlaybackCommandRejected {
    PlaybackCommandId    id;
    PlaybackRejectReason reason;
};
```

and state that it is published on the same ordered channel as status. Noting
that the target document's `PlaybackEvent` gains a matching alternative is
worth one clause, but that edit belongs to the target document, not here.

### F2 — `OpenSource` has no in-flight phase, and the Play/Pause rules contradict its outcome

*Sections: State-transition ownership, `OpenSource` row.*

The row reads:

> increment generation, replace the context, load and seek to source-time zero,
> then enter `Stopped` when the first frame is ready.

No phase is named for the interval between "load and seek" and "enter
`Stopped`". Every other multi-step command names its intermediate phase —
`Play` names `Prerolling`, `Seek` names `Seeking`, `InstallSnapshot` says
"returns through preroll".

This is not only an omission; it produces a contradiction under the most
natural reading. If the loading interval is `Seeking` or `Prerolling`, then the
`Play` row applies:

> During `Seeking` or `Prerolling`, pending intent becomes `Playing`.

so a `Play` arriving while a source loads would complete in `Playing`,
contradicting the `OpenSource` row's "then enter `Stopped`". `Pause` and `Seek`
have the same interaction. The window is real: `OpenSource` always waits for
media, and a user double-clicking an asset then hitting Play lands in it.

**Minimal correction.** One sentence in the `OpenSource` row naming the
in-flight phase and its intent, for example: *"While loading, the session is in
`Seeking` with pending intent fixed to `Stopped`; `Play`, `Pause` and `Seek`
arriving during an `OpenSource` load do not alter that intent."* Any phase
choice works provided the pending-intent exception is stated.

## Non-blocking editorial improvements

**N1 — `SourceTimestamp` has no `zero()`.** ADR-001's five origin constants omit
it, so "source-time zero" is constructible only inside a named helper. Nothing
is blocked; recorded so it is not rediscovered when the helper is implemented.

**N2 — Natural completion and generation.** `Stop` increments generation and
invalidates pending work; natural completion is not stated to do either, though
frames decoded beyond the end are equally stale. One clause, or an explicit
hand-off to ADR-003's invalidation scope.

**N3 — Acceptance criterion 9 remains subjective.** "Without visible redesign"
has no pass condition. Criterion 14 now protects the one behaviour that was at
risk, so binding criterion 9 to "the existing Qt widget tests pass unmodified"
would make it checkable.

**N4 — Criterion 15 tests something the compiler already enforces.** Since
ADR-001 makes general `SourceTimestamp` arithmetic ill-formed, "rather than
general arithmetic" cannot fail to compile. Reword to test the helper's clamping
and zero-end behaviour, which is the part that can actually be wrong.

**N5 — Migration step 5 reads oddly beside the new-path rule.** "Remove MFC
timer advancement after regression parity is established" implies advancement
persists, while the UI-timers section says the new path never advances. Both are
true — step 5 concerns the legacy path — but a clause saying so avoids the
apparent conflict.

**N6 — `PlaybackError` is undefined.** Acceptable as an opaque identity type at
this level, but since criterion 11 requires "a reason", one clause stating that
the error taxonomy belongs to the decoder/ADR-003 layer would close the loop.

## 11. Are the acceptance criteria objective and testable?

Criteria 1 through 8 and 10 through 14 are objective and testable under MSVC
C++17 with the fake clock that the injected `IPlaybackClock` now guarantees and
with fake media ports. Criterion 13 in particular is the one that proves the
ADR's thesis, and it is checkable with a spy or an assertion build.

Two exceptions, both already listed: criterion 11 is untestable until F1 gives
rejection a channel, and criterion 9 is subjective (N3). Criterion 15 is
testable but tests the wrong half (N4).

## 12. Contradictions, undefined transitions, ADR-001 reopening, misassignment

- **Internal contradiction:** one, F2.
- **Undefined reachable transition:** one, F2 (the same cell).
- **Reopened ADR-001 decision:** none. Section 4 explains why the source-progress
  helper stays inside ADR-001's existing adapter allowance.
- **Responsibility assigned to the wrong future ADR:** none. Renderer
  acknowledgement and non-playhead frame publication go to ADR-003, re-anchoring
  and rate mathematics to ADR-004, thread joining and destruction to ADR-005.
  Each deferral is stated at the point where a reader would otherwise assume
  ADR-002 answers it.

## Is ADR-002 implementation-ready?

**Yes, after F1 and F2.** The state machine is otherwise complete for milestone
1, the async contract is closed, the authority boundaries are unambiguous, and
the migration flag makes ownership exclusive rather than synchronised. F1 adds
one small type; F2 adds one sentence to one table row.

## Status change

**ADR-002 may change from Proposed to Accepted once F1 and F2 are applied.**

Neither requires a design decision to be revisited. Recommended sequence:

1. Declare `PlaybackCommandRejected` and state its publication channel (F1).
2. Name `OpenSource`'s in-flight phase and its pending-intent exception (F2).
3. Apply N1 through N6 in the same pass if convenient.
4. Flip the status line and update the status column in
   [`../decisions/README.md`](../decisions/README.md).
5. Implementation issues may then depend on ADR-002.

## Reviewer's note

Two positions from earlier rounds were withdrawn during this series and are
recorded rather than dropped: review 1's `SourceTimestamp sourceDuration`, which
typed a duration as an instant, and review 1's separate `source` plus `position`
members, which the owner's single `PlaybackContext` variant improved on by
making the invalid state unrepresentable instead of merely testable.
