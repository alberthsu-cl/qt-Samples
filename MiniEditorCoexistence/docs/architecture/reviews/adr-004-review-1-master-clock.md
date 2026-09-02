# ADR-004 Review 1 — Audio/Monotonic Master-Clock Policy

Verdict: **Accept with revisions**

Round: 1 (original review)

Primary document:
[`../decisions/0004-audio-monotonic-master-clock-policy.md`](../decisions/0004-audio-monotonic-master-clock-policy.md)
(Status: Proposed)

Reviewed against [ADR-001](../decisions/0001-strong-media-time-domains.md),
[ADR-002](../decisions/0002-playback-session-is-the-state-authority.md), and
[ADR-003](../decisions/0003-immutable-playback-snapshots-and-generation-gated-presentation.md)
(all three Accepted, closed), and
[`../target-playback-architecture.md`](../target-playback-architecture.md).

## Verdict

The clock/anchor mechanics are correct and consistent with the series: every
type name matches ADR-001 exactly — including `MasterClockTime`, whose own
comment in ADR-001 already says "instant measured by `IPlaybackClock`,"
meaning ADR-004's central interface is one ADR-001 explicitly anticipated by
name. The re-anchor/generation split is drawn correctly (re-anchoring retimes;
generation invalidates async work; not every re-anchor needs to invalidate
work, and the document says so explicitly). Deferrals to ADR-005 are placed
correctly and nothing already Accepted is reopened.

One gap blocks acceptance: **the document never states which `PlaybackSource`
alternative it governs.** Every equation, every re-anchor trigger, and every
acceptance criterion is written as if there is one playback position derived
from one clock/anchor pair — but ADR-002 already established that milestone-1
*source-asset* preview position is "adopted from backend observations," not
computed from a clock and anchor. ADR-004 needs to say explicitly whether it
governs sequence preview only, and if so, what (if anything) source preview
does with `IPlaybackClock`. Without that sentence, the re-anchor trigger list
reads as incomplete — it lists "installing a new sequence snapshot" but has no
parallel entry for opening a new source, and a reader has no way to tell
whether that is an intentional scope boundary or an oversight.

Ten non-blocking items would sharpen precision, naming consistency, and
acceptance-criteria testability without touching the decision itself.

## Findings

| ID | Finding | Classification |
| --- | --- | --- |
| B1 | Document never scopes itself to sequence preview vs. source preview | **Blocking** |
| N1 | `PlaybackAnchor.playbackRatePercent` duplicates `PlaybackStatus.ratePercent` with no stated derivation rule | Non-blocking |
| N2 | Pseudocode uses `anchor.rate`; the struct field is named `playbackRatePercent` | Non-blocking |
| N3 | "Audible audio is active" is load-bearing and never defined | Non-blocking |
| N4 | "Underflow" names two different conditions (video-frame vs. audio-buffer) without disambiguation | Non-blocking |
| N5 | Audio-underflow *behavioral* policy is unstated, asymmetric with the thorough video policy | Non-blocking |
| N6 | No real-time-safety statement for the audio callback (no block/allocate) | Non-blocking |
| N7 | Acceptance criterion 2 has no observable milestone-1 behavior to test against | Non-blocking |
| N8 | Acceptance criterion 9 is inspection-like, unlike ADR-002's equivalent | Non-blocking |
| N9 | "Every clock sample … carries" identity overstates what a synchronous read needs | Non-blocking |
| N10 | The pause-freeze value's capture moment is asserted, not described | Non-blocking |
| — | Type names, operator use, `IPlaybackClock` naming | Resolved by ADR-001 |
| — | Phase semantics, generation-invalidation citation, steady-clock exception | Resolved by ADR-002 |
| — | Work identity split, no reopening of accepted snapshot/generation rules | Resolved by ADR-003 |
| — | Command-queue ownership, worker lifetime, callback shutdown | Correctly deferred to ADR-005 |

## Master-clock ownership and audio/monotonic selection

The policy statement itself is right and matches the target document's
already-Accepted invariant 7 exactly: audio-master when audible audio is
active, monotonic otherwise, with milestone 1's `steady_clock`-for-both
exception stated as an explicit, observable exception rather than a hidden
behavior — "It is observable and testable, rather than an implicit behavior
hidden in `QMediaPlayer`" is a genuinely good sentence, and it correctly cites
ADR-002 as the origin of that exception rather than re-deciding it.

What is missing is scope. See **B1**.

## Strong time-domain usage and conversion rules

Fully correct against ADR-001, and this is the strongest part of the
document. Every type — `FrameRate`, `TimelineFrame`, `SourceTimestamp`,
`SequenceTime`, `SequenceDuration`, `MasterClockTime`, `ClockDuration` — is
used exactly as ADR-001 defines it, with no stale or retired name (no
`PresentationTime` anywhere). The conversion pipeline:

```text
elapsedClock = clock.now() - anchor.masterClock          MasterClockTime - MasterClockTime -> ClockDuration
elapsedSequence = sequenceElapsedFor(elapsedClock, rate)  ClockDuration -> SequenceDuration
sequenceTime = anchor.sequenceTime + elapsedSequence      SequenceTime + SequenceDuration -> SequenceTime
timelineFrame = frameAtSequenceTime(sequenceTime, rate)   SequenceTime -> TimelineFrame
```

is exactly ADR-001's permitted operator surface, in exactly this order, with
no field ever crossing domains directly. `frameAtSequenceTime()`'s
nonnegative-input precondition is satisfied by construction here: a monotonic
clock plus "re-anchor at every clock swap" means `elapsedClock` cannot be
negative in production, and the document does not need to add a clamp to
make that true — it already follows from the rules stated.

## Anchor and re-anchoring behavior

The `PlaybackAnchor` triple and its atomic replacement are sound, and the
document draws a distinction the rest of the series has needed care to get
right: re-anchoring (retiming) and generation-advancing (invalidating async
work) are separate mechanisms, and not every re-anchor trigger also
invalidates work. "Re-anchoring never changes the `PlaybackSession` authority
or creates a second transport state" is the correct guard, and the six listed
re-anchor triggers are individually well-chosen — except that, read against
ADR-002, one is conspicuously missing. See B1.

## Pause, seek, rate change, stop, and natural completion semantics

All five are consistent with ADR-002's per-phase table; none are
contradicted. Pause correctly does *not* appear as a re-anchor trigger
(nothing needs recomputing while frozen); resuming from `Paused` correctly
does ("entering `Playing` after `Prerolling` or `Paused`"). Natural completion
is described generically — "enters the existing stopped/completed policy" —
rather than restating ADR-002's two-case split, which is the right amount of
restraint; a paraphrase risks drifting from the original wording, and this
document does not need to repeat it to be correct.

One place is asserted without describing the mechanism: "Pause freezes the
relationship between clock and sequence time" does not say how the frozen
value is captured at the pause instant. See N10.

## Audio/video synchronization and underflow policy

The video side is thorough: bounded queue, wait-when-early, present-not-later,
drop-superseded-late, and underflow-without-moving-the-clock are all stated
and each is independently testable with a fake clock and fake decode results.
"The audio callback never waits for a video frame" is the one invariant that
matters most here, and it is stated plainly.

The audio side is thinner in two ways, both non-blocking given milestone 1 has
no real audio-device clock yet: the word "underflow" is used for both a
video-frame condition and an audio-buffer condition without ever
distinguishing them (N4), and audio underflow's own *behavior* — what happens
during it, not just what happens when resuming from it — is never stated,
unlike video's (N5).

## Stale-result identity interaction with ADR-002 and ADR-003

No contradiction, and one genuinely good cross-check passed: ADR-004 cites
generation advancing "under ADR-002/ADR-003," correctly attributing natural
completion to ADR-003's *extension* of ADR-002's list rather than to ADR-002
alone — which is exactly the corrected citation ADR-003's own review series
required of it. ADR-004 is consistent with the already-corrected text, not
the original.

One sentence overstates what needs tagging: "Every clock sample and scheduled
decode/composition request carries the active `PlaybackSessionId`,
`PlaybackGeneration`, and sequence snapshot identity" — a raw `clock.now()`
call is a synchronous, engine-thread-only read; it does not cross a thread
boundary and is never compared against a "current" state asynchronously, so
it does not need identity of its own. What needs `PlaybackWorkIdentity` /
`SequenceWorkIdentity` is the *scheduled work* the clock reading drives, which
is exactly what ADR-003 already defines. See N9.

## Thread and framework boundaries

Correct and correctly scoped: the engine thread owns phase, anchor,
generation, and clock selection; audio callbacks and decoder workers report
observations and never mutate authority; command-queue ownership, worker
lifetime, and shutdown are explicitly left to ADR-005. Given "the audio
callback never waits for a video frame" already shows awareness that an
audio callback has different constraints than an ordinary worker, the
document stops one sentence short of the constraint that actually matters
most for that thread: real-time safety (no blocking, no allocation, no lock
contention with the engine thread). See N6.

## Objective and testable acceptance criteria

Seven of nine are directly testable under MSVC C++17 with a fake clock and
fake media ports: 1, 3, 4, 5, 6, 7, and 8 each name a concrete, checkable
behavior. Two need rewording, not new content:

- **Criterion 2** ("Audio-master and monotonic-clock selection follow the
  policy above") has no observable difference to test against in milestone 1,
  because decision point 3 makes both branches use the identical
  `steady_clock` implementation. What is actually testable now is that the
  *selection logic* runs and *triggers a re-anchor* on an audible-audio
  transition — not that the resulting timing differs, because it does not
  yet. See N7.
- **Criterion 9** ("The Qt bridge remains a clock adapter; it does not become
  a second transport authority") is inspection-like the way earlier ADRs'
  "no visible redesign" criteria were, before the series learned to make them
  concrete. ADR-002's criterion 13 — "tests prove that no legacy
  `EditorSession` playback mutator is called" — is the model to follow here.
  See N8.

## Contradictions, missing invariants, or scope expansion

**No contradiction** with any Accepted decision in ADR-001, ADR-002, or
ADR-003. **No scope expansion**: `PlaybackAnchor`, `IPlaybackClock`, and the
re-anchor/generation split are all sized to what milestone 1 needs, and every
deferral (audio latency parameters, drift-correction algorithm, source-media
time bases, ADR-005's thread/shutdown mechanics) is placed at a boundary the
series has already established.

**One missing invariant**, which is B1: the document is written as if it
governs one playback position for one `PlaybackSession`, without ever stating
whether that position is sequence-preview-only, or how source-asset preview —
which ADR-002 already scoped differently — relates to `IPlaybackClock` at
all.

## B1 — the document never scopes itself to a `PlaybackSource` alternative

*Whole document; most visible in the re-anchoring rules list.*

`PlaybackSource`, `SourceAssetPreview`, and `SequencePreview` do not appear
anywhere in ADR-004. Every equation uses `SequenceTime` and
`snapshot.frameRate`, which only make sense for sequence preview — a bare
source asset has no snapshot and no sequence frame rate. Yet the document's
opening frames its scope as "how a session turns elapsed time into a sequence
position when audio and video are both active," without saying that this
excludes, or specially handles, source-asset preview.

The gap is concrete, not merely stylistic. The re-anchor trigger list
includes "installing a new sequence snapshot" but has no equivalent entry for
opening a new source, even though ADR-002's `OpenSource` command is exactly
as significant a baseline change as `InstallSnapshot` — a completely new
position, source, and starting point. Reading ADR-002 resolves the apparent
omission: milestone-1 source preview position is "adopted from backend
observations," not computed via a clock-plus-anchor equation at all — so
`OpenSource` correctly needs no anchor, because source preview never uses the
anchor mechanism in the first place. But **ADR-004 never says this**. A reader
arriving at ADR-004 alone, without independently reconstructing this from
ADR-002's text, cannot tell whether the missing trigger is this deliberate
scope boundary or an error.

**Minimal fix.** One paragraph, most naturally placed right after the
Decision section's audience-setting sentence: state that this ADR's anchor
and re-anchoring machinery governs `SequencePreview` position; that milestone-1
`SourceAssetPreview` position continues to be adopted from backend
observations per ADR-002 and does not consume `IPlaybackClock`; and that a
future ADR unifying the two paths (referenced already as a possibility in
ADR-002: "ADR-004 may later unify source and sequence clock advancement
without changing this ownership boundary") is out of scope for milestone 1.
No new decision is required — ADR-002 already made this decision. ADR-004
only needs to say so.

## Minimal exact edits

| # | Section | Edit | Ref |
| --- | --- | --- | --- |
| 1 | Decision, or a new paragraph immediately after it | State the sequence-preview-only scope and source preview's continued observation-based position, per ADR-002. | **B1** |
| 2 | Clock contract, `PlaybackAnchor` | State that `playbackRatePercent` is captured from the session's rate preference at each re-anchor and is never set independently. | N1 |
| 3 | For a playing session (equations block) | Change `anchor.rate` to `anchor.playbackRatePercent`, matching the struct field name. | N2 |
| 4 | Audio-master and video policy | Define "audible audio is active," or state that milestone 1 does not require testing the boundary precisely because both clock branches share an implementation. | N3 |
| 5 | Audio-master and video policy | Distinguish "video-frame underflow" from "audio-buffer underflow" as named terms at each usage site. | N4 |
| 6 | Deferred decisions | Add audio-underflow behavioral policy (not only its parameters) to the list of items a follow-up ADR may need to settle. | N5 |
| 7 | Identity and thread boundaries | Add one sentence: the audio callback thread must not block, allocate, or contend with the engine thread; it hands off observations through a lock-free or otherwise non-blocking path. | N6 |
| 8 | Acceptance criteria 2 | Reword to test that clock-selection logic runs and triggers a re-anchor on an audible-audio transition, not a numeric timing difference. | N7 |
| 9 | Acceptance criteria 9 | Reword to a concrete assertion, matching ADR-002 criterion 13's form. | N8 |
| 10 | Identity and thread boundaries | Narrow "every clock sample … carries" to "every scheduled decode/composition request derived from a clock reading carries." | N9 |
| 11 | Re-anchoring rules | State that the frozen position on Pause is captured by evaluating the anchor equation once at the pause instant, then held fixed. | N10 |

## Reviewer's note

This document gets the hard part right — the type discipline, the
re-anchor/generation distinction, and the clock-swap abstraction are all
correct on first read, and the `IPlaybackClock` interface is literally the
seam ADR-001 already named in its own comments. The one thing it needs is not
a design change: it is the same kind of missing scope sentence that ADR-003
needed for `PlaybackMediaDescriptor` and that ADR-002 needed for its
`PlaybackStatus` variant — a decision already made elsewhere in the series,
not yet carried into this document's own text.
