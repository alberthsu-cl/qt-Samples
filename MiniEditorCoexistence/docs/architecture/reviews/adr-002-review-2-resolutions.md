# ADR-002 Review 2 — Owner Resolutions

Verdict: **Accept with blocking revisions**

Round: 2 (resolution review)

Scope: the owner's proposed resolutions to B1–B8. ADR-002 is not yet revised;
this review assesses the resolutions as stated.

Reviewed against [ADR-001](../decisions/0001-strong-media-time-domains.md)
(Accepted, closed),
[ADR-002](../decisions/0002-playback-session-is-the-state-authority.md),
[review 1](adr-002-review-1-playback-authority.md),
[`../current-playback-architecture.md`](../current-playback-architecture.md),
and [`../target-playback-architecture.md`](../target-playback-architecture.md).

## Position

Five of eight blockers are cleanly resolved and two of the resolutions are
better than what review 1 recommended. The clock boundary, the asynchronous
command contract, failure recovery and migration ownership are all closed.

Three items block: one original finding is still open because its resolution
solves the wrong problem, and two new contradictions were introduced by the
resolutions themselves — one against ADR-001, which is closed and must stay
closed, and one against documented product behaviour.

## 1. Classification of B1–B8

| ID | Original finding | Classification |
| --- | --- | --- |
| B1 | `PlaybackStatus` cannot populate the promised cache | **Resolved** — see B9 for a consequence |
| B2 | `TimelineFrame` for source preview invents a grid | **Resolved** |
| B3 | Clock boundary unstated | **Partially resolved** |
| B4 | Synchronous façade vs asynchronous commands | **Resolved** |
| B5 | Command set misses editor-driven pause exit | **Still open** |
| B6 | `Failed` has no defined exit | **Resolved** |
| B7 | Six phase/command pairs undefined | **Partially resolved** |
| B8 | Migration permits two authorities | **Resolved** |

### B1 — Resolved

All three parts are answered: `sourceEndTime` / `sequenceDuration` give the
transport slider its range, `std::optional<PlaybackError>` makes the error a
value rather than prose, and deferring the presented frame to ADR-003 is exactly
the right split. Dropping the separate `PlaybackSource` member in favour of the
context variant is a genuine improvement over review 1's proposal — see
section 3.

### B2 — Resolved

`SourcePreviewStatus` reports `SourceTimestamp`, its natural domain. No implicit
30/1 grid, and ADR-001 stays closed.

### B3 — Partially resolved

Injection, the fake clock for tests, the `(MasterClockTime, SequenceTime)`
anchor and the prohibition on calling `steady_clock` directly are all correct
and sufficient for ADR-004 to attach to.

**What remains open:** the anchor pair is expressed entirely in the *sequence*
domain, but `SourcePreviewStatus` carries a `SourceTimestamp`. There is no
stated way to advance a source-preview position from a
`(MasterClockTime, SequenceTime)` anchor, and ADR-001 provides no conversion
between `SequenceTime` and `SourceTimestamp` outside `sourceTimestampFor()`,
which requires a `ClipTimeMapping` — a *timeline clip* mapping that a bare
source preview does not have.

Milestone 1 needs one sentence choosing between:

- a source-domain anchor, `(MasterClockTime, SourceTimestamp)`; or
- source-preview position being adopted from accepted backend observations
  rather than clock-advanced, with ADR-004 unifying the two later.

The second is closer to how `QMediaPlayer` actually behaves today and is the
smaller commitment. Either way, "the session decides" still holds — but which
mechanism produces the source-preview number must be written down.

### B4 — Resolved

Non-blocking submission, `PlaybackCommandId`, acknowledgement through
`lastAppliedCommandId`, explicit rejection with a reason, and monotonic
`statusSeq` with consumer-side discard together close the finding. Making the
MFC timer a status-reading heartbeat removes the original conflict.

One editorial consequence is unstated; see N4.

### B5 — Still open

The resolution declines `ExitPausedForEdit`, which is defensible. But its
replacement — *"Selecting another timeline clip while paused issues an explicit
Seek to that clip's timeline position and preserves Paused"* — solves a
different problem from the one the existing call solves, and introduces a
behaviour regression. This is B10 below.

### B6 — Resolved

Stop clears the error, `OpenSource` and `InstallSnapshot` recover through a new
generation, `Shutdown` is always valid, everything else is explicitly rejected.
Complete and unambiguous.

### B7 — Partially resolved

The rules cover most of the grid and settle the two hardest cases correctly
(scrubbing seeks, pending intent). Four cells remain undefined; see section 5.

### B8 — Resolved

The flag now chooses exactly one authority, legacy mutators are not called on
the new path, and `PreviewStateResolver` derives context from `PlaybackStatus`
rather than focus. That last point closes review 1's N1 as well. One
consequence to name; see N3.

## 2. Newly introduced blocking contradictions

### B9 — Source-preview progress is not computable under ADR-001

ADR-001's permitted arithmetic covers three domain triples —
`TimelineFrame`/`FrameCount`, `SequenceTime`/`SequenceDuration`, and
`MasterClockTime`/`ClockDuration`. **`SourceTimestamp` appears in none of
them.** It has comparison, granted by F1, and no arithmetic at all. There is
also no `SourceDuration` type and no `SourceTimestamp::zero()`.

`SourcePreviewStatus` therefore hands the UI two `SourceTimestamp` values and no
legal way to combine them. A transport slider needs a fraction —
`sourceTime / sourceEndTime` — and both the subtraction and the division are
ill-formed. The status is correct in domain terms but not renderable.

ADR-001 is Accepted and closed, so the fix must land in ADR-002. Two options,
both small:

- state that the UI adapter converts `SourceTimestamp` to a display quantity at
  the framework boundary, which ADR-001 already permits for "framework
  adapters" — but ADR-001's boundary bullet currently names only
  `TimelineFrame` for "UI sliders and timecode", so ADR-002 must say the source
  case explicitly; or
- have ADR-002 define one named helper, for example
  `int sourceProgressPermille(SourceTimestamp position, SourceTimestamp end)`,
  keeping the arithmetic inside a named conversion exactly as ADR-001 requires.

The second is more in the spirit of ADR-001 and does not widen the type set.
Without one of them, the first implementer will either add `SourceTimestamp`
arithmetic — reopening a closed ADR — or reach for raw values ad hoc.

### B10 — Resolution 4 contradicts documented behaviour and mislayers presentation as transport

Two problems, and the second is the more structural one.

**It changes documented behaviour.** `README.md` records a deliberate design
decision:

> Selecting a clip no longer resets the head, so the same ruler position can
> immediately enable Split.

Seeking to the selected clip's position moves the head, which breaks exactly
that: after selecting a clip you can no longer Split at the ruler position you
had chosen. ADR-002 acceptance criterion 9 requires the existing UI to operate
"without visible redesign"; moving the playhead on selection is a visible
behaviour change, so the resolution conflicts with the ADR it belongs to.

**It solves the wrong problem.** The existing call is not expressing user
transport intent. Its own comment says so:

> A clip click is an explicit editing action. End a frozen paused preview before
> Selection is notified; otherwise the decoder sees the old playhead clip during
> that notification and never loads the newly focused clip's first frame.

The need is *presentation*: show the newly focused clip's first frame. The
current code achieves that by clearing `isPaused` only because preview is
derived from playback state today — the very coupling ADR-002 exists to remove.

`PreviewStateResolver` already implements the correct behaviour independently of
the playhead: while timeline playback is stopped, it previews the focused
placement even when the playhead is elsewhere. Under ADR-002 plus ADR-003 that
stays a presentation concern, and clip selection should issue **no transport
command at all**.

Recommended resolution: state that selecting a timeline clip changes the
preview's presentation target, not the transport position; the playhead is
unchanged and the phase is unchanged. Frame publication for a non-playhead
presentation target is ADR-003's business. This also removes the need for both
`ExitPausedForEdit` and the Seek.

### B11 — `HoldLastFrame` conflicts with the Stopped-implies-start invariant

Resolution 6 defines Stop as "enter Stopped, and move to the context's defined
start position", and resolution 5 repeats it for recovery from `Failed`. Stopped
therefore *means* "at the start".

Resolution 4 gives source preview the completion policy `HoldLastFrame`. A
source that has finished and is holding its last frame is not at the start, so
its phase cannot be `Stopped` without contradicting the invariant — yet no
resolution says which phase it is.

The consistent answer is **`Paused` at `sourceEndTime`**, which also matches the
resolution's own treatment of Seek-from-Stopped (section 5). Sequence preview's
`ReturnToStart` correctly lands in `Stopped`. Say both explicitly, because the
asymmetry is the whole point of having a policy value.

## 3. Critique of the single PlaybackContext variant

**It prevents mismatched domains inside a status value, and it does so better
than review 1's proposal did.**

Review 1 recommended separate `source` and `position` members with a stated
invariant that their alternative indices match. The owner's version removes the
possibility instead of testing for it: a `SequenceId` cannot be paired with a
`SourceTimestamp` because they live in different alternatives of one variant.
Making the invalid state unrepresentable is strictly better than making it
testable, and one recommended acceptance criterion from review 1 is now
unnecessary.

Three limits worth recording.

**It constrains one value, not the boundary.** ADR-002's Decision section still
declares `PlaybackSource = std::variant<SourceAssetPreview, SequencePreview>`
for the command side. There are now two parallel variants that must stay in
correspondence, and a future third source kind must be added to both. Nothing
enforces that. Recommend stating that `PlaybackContext` is derived from the
active `PlaybackSource` plus resolved media metadata, and that their
alternatives correspond one-to-one — see N1.

**`phase` and `error` remain independently variable.** "An error value is
present if and only if phase is `Failed`" is prose. It could be made
unrepresentable by carrying the error inside a `Failed` alternative, but that
restructures the phase enum for one invariant. A tested invariant is the right
trade for milestone 1; just keep it as an acceptance criterion.

**`SourceCompletionPolicy` sits in status but reads like an input.** It is
chosen when the source is opened, not discovered during playback. It is
reasonable to echo it in status so the UI can explain end-of-media behaviour,
but it should originate on `OpenSource`. See N7.

## 4. Is `sourceEndTime` semantically better than a duration?

**Yes, and review 1 was wrong here — I should correct it explicitly.**

Review 1 proposed `SourceTimestamp sourceDuration`. That types a *duration* as
an *instant*, which is precisely the position/duration confusion that ADR-001's
own B3 was accepted to eliminate. The owner caught a domain error in the review.

`sourceEndTime` is also the only ADR-001-legal option available. ADR-001 defines
no `SourceDuration` type, so a source-media length has no legal representation;
expressing the extent as a second *instant* avoids needing one and keeps the
closed ADR closed. That is the right instinct.

Two riders:

- It makes B9 unavoidable rather than optional. Two instants with no subtraction
  is a complete description and an unusable one; the named helper in B9 is what
  makes the choice work.
- It carries an implicit assumption: source preview starts at source-time zero.
  True while source preview means "the whole asset". If trimmed source preview
  ever appears, `sourceStartTime` is needed too. Worth one sentence now — see
  N2.

## 5. Review of the transition rules

### Seek from Stopped entering Paused — correct, and for a better reason than stated

This matches the target document's existing state-machine contract:

> ```text
> Stopped
>   play  -> Prerolling -> Playing
>   seek  -> Seeking    -> Paused
> ```

It is also *forced* by the resolutions themselves. Since Stop is defined as
moving to the context's start position, `Stopped` implies "at the start". A seek
that stayed in `Stopped` would produce a Stopped session at a non-start
position, making the phase ambiguous. Paused is the only consistent
destination.

One consequence to note: milestone 1 has **no phase meaning "not playing,
positioned somewhere, nothing decoded"**. Clicking the ruler while stopped —
common in this editor — now enters `Paused` and therefore implies decode and
preroll semantics. That is acceptable and simpler, but the ADR should say
whether Seek-from-Stopped passes through `Prerolling`, i.e. whether `Paused`
always implies a presented frame. See N6.

### Generation changes — correct, including the scrubbing case

"Every accepted Seek immediately increments generation, including repeated
scrubbing seeks" is the right call and answers review 1's B7 directly. It costs
a generation per slider sample and buys a single rule with no settling
heuristic. The existing codebase already validates the pattern:
`PreviewSeekRequest::requestId` with the comment that a backend "must accept
only the result belonging to its newest request".

Note for ADR-003: generation-per-scrub-sample makes generation a high-rate
counter, so its type must not be a narrow integer and any per-generation
bookkeeping must be O(1).

### Pending intent during Seeking and Prerolling — well specified

Play, Pause and Seek all define their effect on a session that is mid-seek or
mid-preroll, and "replace the target while preserving pending transport intent"
is exactly the rule needed for scrubbing during playback. This is the strongest
part of resolution 6.

**One gap:** `InstallSnapshot` does not define its behaviour during `Seeking` or
`Prerolling`. Its rule preserves "previous transport intent" for Playing, Paused
and Stopped only. An edit committed while a seek is in flight is reachable in
milestone 1 — the editor is live during preview — so this cell needs an answer.
The consistent one is that pending intent is preserved exactly as Seek does.

### Recovery from Failed — complete

Nothing to add. Stop, OpenSource, InstallSnapshot and Shutdown are defined;
everything else is explicitly rejected rather than ignored.

### Completion policies — see B11

The policies are right; the resulting phase for `HoldLastFrame` is missing and
contradicts the Stopped-implies-start invariant.

### Remaining undefined cells

| Cell | Status |
| --- | --- |
| `InstallSnapshot` during `Seeking` / `Prerolling` | **Undefined** — reachable; preserve pending intent |
| Source completion phase under `HoldLastFrame` | **Undefined** — B11 |
| Resulting phase after `Shutdown` | **Undefined** — N5 |
| Does `Paused` always imply a presented frame? | **Undefined** — N6 |

Everything else in the grid from review 1 section 4 is now answered.

## Non-blocking improvements

**N1 — Two parallel variants.** State that `PlaybackContext` is derived from the
active `PlaybackSource` and that their alternatives correspond one-to-one, so a
future source kind cannot be added to only one.

**N2 — Implicit source start.** State that source preview covers the whole
asset, so its start is source-time zero; `sourceStartTime` arrives with trimmed
source preview.

**N3 — The legacy cache re-introduces the conversion B2 removed.** Populating
`PlaybackState::currentFrame` from a `SourcePreviewStatus` requires converting
source time to frames at a fixed rate. That is legal at a UI adapter, but it
should be named as a display-only, lossy conversion so B2's fix is not quietly
undone in the adapter.

**N4 — The façade return value is no longer a timer directive.** With a
constant-interval heartbeat driven by published status,
`IPlaybackBackend::executeCommand()`'s `PlaybackClockAction` return has no
remaining job on the new path. Say so, or `MainFrame::applyPlaybackClockAction()`
will keep consuming it.

**N5 — `Shutdown` has no resulting phase.** "Closes command acceptance" is a
queue property; the phase enum has no terminal member. Either add one or state
that the phase is unchanged and acceptance is a separate flag.

**N6 — Does `Paused` imply a presented frame?** Relevant to Seek-from-Stopped
and to whether `Paused` can be entered without preroll.

**N7 — `SourceCompletionPolicy` should originate on `OpenSource`** and be echoed
in status.

**N8 — `statusSeq` reset semantics.** "Monotonically increasing" needs a scope:
per session, restarting for a new `PlaybackSessionId`. A consumer that keeps the
highest seq across sessions would otherwise discard the first statuses of the
next one.

## 6. What belongs in ADR-002 versus later ADRs

**Stays in ADR-002:** the full command/phase table including the four undefined
cells; `Failed` recovery; the completion-policy phases; the command,
acknowledgement and rejection contract; `statusSeq` ordering and reset;
`PlaybackContext` and its correspondence to `PlaybackSource`; the B9 source
progress helper or adapter statement; the clock *injection* boundary and the
anchor's representation for both contexts; migration ownership.

**ADR-003:** presented-frame reporting and renderer acknowledgement; snapshot
construction; what a generation invalidates and how pending work is discarded;
frame publication for a non-playhead presentation target, which B10 needs;
queue bounds. Also the generation-rate note from section 5.

**ADR-004:** clock advancement and rate mathematics; re-anchoring on pause, seek
and rate change; audio-derived clocking; unifying source-preview and
sequence-preview position advancement if B3 is resolved by the observation
route.

**ADR-005:** worker lifecycle, thread join, `QObject` destruction — the
mechanics behind `Shutdown`, whose *state-machine* contract stays here.

## 7. Minimal exact edits to make ADR-002 implementation-ready

| # | Section | Edit | Ref |
| --- | --- | --- | --- |
| 1 | Decision | Replace `PlaybackStatus` with the resolution's version, including `PlaybackContext`. | B1, B2 |
| 2 | Decision | State that `PlaybackContext` is derived from `PlaybackSource` with one-to-one alternatives. | N1 |
| 3 | Decision | Add the source-progress helper, or state that the UI adapter converts `SourceTimestamp` for display. | **B9** |
| 4 | Decision | State that source preview starts at source-time zero. | N2 |
| 5 | Decision | Add the injected `IPlaybackClock`, the `(MasterClockTime, SequenceTime)` anchor, and how a *source-preview* position is produced. | **B3** |
| 6 | Decision (`PlaybackCommand`) | Keep the command set unchanged; add no pause-exit command. | B5 |
| 7 | Separation from EditorSession | State that selecting a timeline clip changes the preview presentation target only — no transport command, no playhead move, no phase change. | **B10** |
| 8 | State-transition ownership | Replace the bullets with the full table; add `InstallSnapshot` during Seeking/Prerolling; define the `HoldLastFrame` completion phase as `Paused` at `sourceEndTime` and `ReturnToStart` as `Stopped`; define `Shutdown`'s resulting phase; state whether `Paused` implies a presented frame. | **B7**, **B11**, N5, N6 |
| 9 | Session identity and generation | State that every accepted Seek increments generation, including scrubbing. | B7 |
| 10 | Player and decoder callbacks are observations | Add the idempotence rule for duplicate and non-transitioning observations. | B7 |
| 11 | UI timers are presentation heartbeats only | State the constant interval and that the façade return value is no longer a timer directive. | N4 |
| 12 | Migration strategy | State mutually exclusive flag ownership, uncalled legacy mutators, the read-only cache and its display-only conversion, and `PreviewStateResolver` consuming status. | B8, N3 |
| 13 | Acceptance criteria | Add the criteria in section 9. | — |

## 8. Verdict

**Accept with blocking revisions.**

Five blockers are closed, two of the resolutions improve on what review 1
proposed, and the deferral boundaries to ADR-003, ADR-004 and ADR-005 are drawn
in the right places. The direction is not in question.

Three items must land before ADR-002 is implementation-ready:

- **B9** — source-preview progress is not computable without reopening ADR-001;
- **B10** — resolution 4 regresses documented behaviour and puts a presentation
  concern in the transport authority;
- **B11** — `HoldLastFrame` has no phase consistent with Stopped-implies-start.

Plus completing B3's source-domain anchor and B7's four remaining cells. None of
these expands milestone 1; B10 shrinks it, by removing a command and a state
change the resolution would otherwise have added.

## 9. Are the acceptance criteria sufficient?

The nine existing criteria remain valid. The resolutions require five additions:

1. Every published status has an `error` engaged if and only if
   `phase == Failed`, and a `PlaybackContext` alternative matching the active
   `PlaybackSource`.
2. A command invalid in the current phase produces an explicit rejection
   carrying its `PlaybackCommandId` and a reason; from `Failed`, only Stop,
   OpenSource, InstallSnapshot and Shutdown are accepted.
3. A duplicate or non-transitioning observation is idempotent, and a consumer
   discards any status whose `statusSeq` is not greater than the last accepted
   one.
4. With the new path enabled, no legacy `EditorSession` playback mutator is
   called — the criterion that most directly proves the ADR's thesis.
5. Selecting a timeline clip while paused leaves phase and playhead unchanged
   and changes only the preview presentation target.

All five are testable under MSVC C++17 with the fake clock and fake media ports
that resolution 2 now guarantees.

## Reviewer's note

One position from review 1 is withdrawn: the recommendation of
`SourceTimestamp sourceDuration`, which typed a duration as an instant and would
have violated ADR-001's own position/duration separation. The owner's
`sourceEndTime` is correct. It is recorded here rather than dropped.
