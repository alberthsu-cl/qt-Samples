# ADR-001 Review 3 — Final Acceptance Gate

Verdict: **Accept with minor editorial changes**

Round: 3 (final acceptance gate)

Reviewed documents, as they stand on 2026-09-02:

1. [`../decisions/0001-strong-media-time-domains.md`](../decisions/0001-strong-media-time-domains.md)
   (Status: Proposed, revised after rounds 1 and 2)
2. [`../target-playback-architecture.md`](../target-playback-architecture.md)
3. [review 1](adr-001-review-1-time-domains.md)
4. [review 2](adr-001-review-2-resolutions.md)

This is an acceptance gate, not a new design pass. Deferred audio-clock,
source-metadata, decoder-seek, nested-sequence and persistence-format work is
not reopened.

## Verdict

The revision closes every finding from rounds 1 and 2. Both documents now agree,
the retired `PresentationTime` leaves no residue in either, and the milestone
exceptions are stated where a reader will actually meet them rather than only in
a review.

One omission gates the status flip: the permitted operator surface never grants
**relational comparison**, yet the scheduler cannot be written without it. That
is finding F1 below. It adds no new decision and changes no conclusion, which is
why the verdict is editorial rather than "Accept with revisions" — but the ADR
declares its operator surface to be the contract and says everything unlisted is
ill-formed, so the gap cannot be resolved by an implementer's good judgement. It
must be filled before the status line changes.

## 1. Original findings B1–B5

| ID | Status | Evidence |
| --- | --- | --- |
| B1 | **Resolved** | `PresentationTime` retired and replaced by `SequenceTime` and `MasterClockTime` with stated origins. The Context section now lists five concepts, separating "an instant measured from sequence zero" from "an instant measured by the playback master clock". |
| B2 | **Deferred, safely** | The Master-clock scope section names the deferral, its reason, and its cost in the same place. `MasterClockTime` is explicitly "not tied by name or representation to `steady_clock`", so ADR-004 can substitute an audio clock without touching engine APIs. |
| B3 | **Resolved** | Positions, lengths, instants and durations are eight distinct types; storage is private; the operator surface is explicit. The `.value` escape hatch is gone. |
| B4 | **Resolved** | Nonnegative preconditions on `TimelineFrame` and `SequenceTime`, floor semantics stated, callers clamp. See section 5 for why this is stronger than it looks. |
| B5 | **Resolved** | One-way conversion, legacy field authoritative, format unchanged. Drift is designed out rather than tested for. |

The B2 deferral is safe because nothing in milestone 1 depends on the deferred
work. The one place it could have leaked — an acceptance criterion demanding
sync that cannot be measured — was closed; see section 8.

## 2. Round-2 findings RB1–RB3

| ID | Status | Evidence |
| --- | --- | --- |
| RB1 | **Resolved** | Invariant 7 now reads "In the target state, audio is the master clock ... Milestone 1 has the documented steady-clock-only exception until ADR-004 removes it." Decision 6 carries the matching exception. The milestone can now pass its own acceptance list. |
| RB2 | **Resolved** | `frameAtSequenceTime()` carries `// precondition: time >= sequence zero`, and the boundary section states that callers clamp a negative intermediate before calling. |
| RB3 | **Resolved** | Closed in three places at once: the non-goals list, the integration-test list ("verifying concurrent playback and lifecycle but not measured A/V synchronization"), and Decision 6's consequence. Acceptance criterion 12 matches. |

RB3 is worth singling out. The failure mode was an acceptance item nobody could
pass or fail. It is now stated as a non-goal, reflected in the test list, and
mirrored in ADR-001 criterion 12 — three mutually reinforcing statements rather
than one buried caveat.

## 3. SequenceDuration and ClockDuration: consistency and MSVC C++17

**Internally consistent.** The four instant/duration pairs are closed under
their own arithmetic, and the only crossings are the two named rate helpers.
There is no path from `ClockDuration` to `SequenceTime` that avoids
`sequenceElapsedFor()`.

**Implementable in MSVC C++17** without difficulty. These are ordinary classes
wrapping an integer or `std::chrono::microseconds`, with explicit constructors
and hand-written operators. No C++20 feature is required: no concepts, no
spaceship operator, no CTAD. `std::optional<SourceTimestamp>` is C++17. The
"64-bit intermediates when comparing products" note for `FrameRate` equality is
plain `std::int64_t` multiplication.

Two implementation consequences follow from private storage, both already
anticipated by the ADR's sentence permitting explicit access from "named
conversion helpers, serialization code, and framework adapters":

- the types are not aggregates, so no brace initialisation and no
  `std::is_trivially_copyable` guarantees for anyone tempted to `memcpy` a
  snapshot;
- adapters need a named accessor rather than `.value`, which is the point.

**The round-2 recommendation is withdrawn.** Review 2 preferred a single shared
`Duration` on grounds that rate-change bugs were speculative. They are not:
target §Scope ships 0.5x, 1.0x, 1.5x and 2.0x preview rates *in milestone 1*, so
a `ClockDuration` added to a `SequenceTime` is a reachable defect in the first
release, not a future one. The owner's split is the correct call and the two
extra types are earned.

## 4. Operator surface

The surface correctly prevents every invalid case it enumerates: adding two
positions or instants, cross-origin assignment, multiplying a `TimelineFrame` by
rate, and both direct duration crossings.

It does **not** block any necessary scheduler calculation. Working through
target Decision 6:

```text
sequence progress   SequenceTime  + sequenceElapsedFor(ClockDuration, rate)
frame lookup        frameAtSequenceTime(SequenceTime, FrameRate)
frame deadline      MasterClockTime + clockElapsedFor(
                        sequenceTimeAtFrameStart(f) - anchorSequenceTime, rate)
```

Every step is a listed operation. The scheduler's anchor is a
`(MasterClockTime, SequenceTime)` pair, which is ADR-004's concern, not this
ADR's.

### F1 — Relational comparison and the origin constants are never granted

**This is the one finding that gates the status flip.**

The permitted-arithmetic block lists only additive operations, and states
"Everything not listed is ill-formed". Relational comparison is nowhere granted.
Yet the scheduler policy in target Decision 6 requires it three times:

> - wait when the next video frame is early;
> - present the newest frame not later than the target time;
> - drop superseded late video frames;

Each is a comparison of two `MasterClockTime` values. ADR-001's own
`frameAtSequenceTime()` precondition, `time >= sequence zero`, is a fourth — and
it also implies a nameable "sequence zero" `SequenceTime`, which the ADR uses in
prose but never defines as a value.

The ill-formed list hints that same-origin comparison is intended to be legal —
it forbids "assigning or **comparing** values from different time origins",
which only makes sense if same-origin comparison exists. But a hint in the
prohibition list is not a grant in the contract.

**Recommended edit**, in the Permitted arithmetic section:

- add `==`, `!=`, `<`, `<=`, `>`, `>=` between two values of the *same* type,
  for all eight types;
- state that comparison across types or origins remains ill-formed, which the
  existing bullet already implies;
- name the origin constants the preconditions depend on — a zero `TimelineFrame`,
  a zero `SequenceTime`, and zero-valued `FrameCount`, `SequenceDuration` and
  `ClockDuration`.

Optionally, unary negation on the three signed types. `FrameCount` negation in
particular is likely at call sites that reverse a delta.

## 5. Frame conversion, floor semantics, preconditions, rate conversion

**Unambiguous, and one part is better than round 2 asked for.**

Requiring nonnegative input to `frameAtSequenceTime()` does more than document a
precondition: on nonnegative values floor and truncation agree, so the C++
signed-division hazard is removed from the implementation rather than merely
tested around. The ADR's own sentence — "This avoids depending on C++ signed
integer division, which truncates toward zero" — is exactly right, and the
policy is stronger for having pushed clamping to the callers where it is
visible.

`sequenceTimeAtFrameStart()` and `sequenceTimeAtNextFrameStart()` make the
half-open interval testable. `playbackRatePercent` is stated positive, which
also removes the divide-by-zero in `clockElapsedFor()`. Rate never mutates a
stored position, duration or trim value.

One gap, non-blocking — see E2: rounding is specified for frame/time conversion
but not for the two rate helpers, whose inputs are explicitly **signed**.

## 6. Legacy sourceInFrame and unchanged persistence

**No drift is possible.** The chain is airtight:

- the legacy field is defined as fixed 30/1-grid units, with the stated
  justification that this matches what the current player already does;
- conversion is one-way at snapshot build;
- editing and saving keep using the original frame field;
- derived timestamps are never written back.

`sourceInFrame / 30` is not exactly representable in microseconds, but the value
is recomputed from the absolute legacy field on every snapshot build, so the
truncation is deterministic and cannot accumulate across load/save cycles.
Acceptance criterion 10 tests precisely this path.

The ADR is also honest about the limit: it "does not claim that the value is an
index in the source file's encoded frame rate". That sentence is what makes the
fixed-30/1 reading safe for 25 fps and 29.97 fps sources.

## 7. Agreement between ADR-001 and the target architecture

**They agree.** `PresentationTime` survives in both documents only in the
sentence announcing its retirement. The target document's type block, role list,
`SequencePlaybackSnapshot::duration` (now `FrameCount`),
`VideoDecodeRequest::deadline` (now `MasterClockTime`), invariants 7 and 9, and
the proposed-ADR list are all consistent with the ADR.

The target document also now defers to the ADR explicitly:

> ADR-001 owns the exact type, arithmetic, rounding, legacy-source, and
> milestone clock contracts. This target document uses those names without
> weakening them.

That sentence is the right structural fix — it prevents the two documents
drifting apart again.

One soft contradiction remains, editorial only; see E1.

## 8. Testability of the acceptance criteria

No criterion is impossible to test. Criteria 2 through 8 and 10 through 12 are
directly automatable, including the compile-fail tests in criterion 2 and the
24-hour conversion tests in criterion 6.

Criterion 12 deserves credit: it asserts concurrent playback, seek,
pause/resume and shutdown while explicitly *not* asserting sample-exact sync.
That is a testable statement of a deferral, which is what RB3 asked for.

Two criteria are verification-by-inspection rather than automated tests. Both
are legitimate but should say so; see E3 and E4.

## 9. Milestone size

**Appropriately small, and smaller in effort than the type count suggests.**

The type set grew from six to eight. That growth is earned, as section 3
explains. Everything else moved toward less work, not more:

- custom 128-bit arithmetic is explicitly excluded;
- no frame-rate UI;
- no project-format change;
- one synthesized in-memory sequence;
- decoder keyframe and frame-selection policy pushed to a separate contract;
- A/V sync measurement pushed to ADR-004.

The four tested rates exercise conversion helpers only. The one long pole is the
compile-fail test suite in criterion 2, which is unavoidable if the type system
is the deliverable.

## Remaining blocking findings

**F1** — Relational comparison operators and the origin/zero constants are
required by the scheduler and by ADR-001's own preconditions, but are not
granted by the permitted operator surface, which declares everything unlisted
ill-formed. Detail and recommended edit in section 4.

No other blocking findings.

## Non-blocking editorial suggestions

**E1 — Decision 6's conceptual block shows the forbidden operation.**
[`../target-playback-architecture.md`](../target-playback-architecture.md),
Decision 6, immediately after "Conceptually:":

> ```text
> timeline position = anchor position
>                   + elapsed master-clock time * playback rate
> ```

Read literally this multiplies elapsed clock time by rate and adds it to a
position — the crossing ADR-001 forbids at type level. It is labelled
"Conceptually", so it is not a type claim, but it is now the only place in
either document that reads as the prohibited form. Naming
`sequenceElapsedFor()` in the block would make the pseudocode agree with the
contract.

**E2 — Rounding for the two rate helpers is unspecified.**
[`0001-strong-media-time-domains.md`](../decisions/0001-strong-media-time-domains.md),
Boundary and rounding policy, specifies rounding for frame/time conversion only.
`sequenceElapsedFor()` and `clockElapsedFor()` take **signed** durations, so
integer division will round toward zero and behave asymmetrically about zero.
The asymmetry is a microsecond and harmless for scheduling waits, but state the
rule — and note that the pair is not round-trip exact, so re-anchoring should
carry the exact anchor rather than recompute it. The re-anchoring policy itself
stays with ADR-004.

**E3 — Criterion 5 names a test of the caller, not the function.**
"Frame-start, next-frame-start, half-open boundary, floor, and explicit
negative-input clamping tests pass." Since `frameAtSequenceTime()` has a
nonnegative precondition, there is no negative-input behaviour to test in the
function. Reword to say that callers clamp before converting, or that the
precondition is checked in debug builds.

**E4 — Criterion 1 is inspection, not a test.** "New playback-engine APIs expose
no unqualified integer frame or time parameters" has no automatable definition
of "unqualified". Either mark it as a review-checklist item alongside criterion
9, or bound it to a named header set.

**E5 — ADR-001 header still reads "Status: Proposed".** Expected while this gate
is open; noted so the flip is not forgotten.

## Ready to change from Proposed to Accepted?

**Yes, once F1 lands.** — *Satisfied; see [Closure](#closure--2026-09-02) below.*

F1 requires adding operators to a list, not making a decision. Nothing else in
the ADR changes, no conclusion is revisited, and the four editorial suggestions
can ride along in the same pass or follow later.

Recommended sequence:

1. Apply F1 to the Permitted arithmetic section.
2. Apply E1 to the target document, and E2 through E4 to ADR-001 if convenient.
3. Flip ADR-001 to **Accepted** and update the status column in
   [`../decisions/README.md`](../decisions/README.md), whose index text still
   says future decisions will cover playback authority — already covered by
   ADR-002.
4. Implementation issues may then depend on ADR-001.

## Closure — 2026-09-02

All items from this gate were applied and verified against the working tree.
ADR-001 is now **Accepted**; the gate is closed and no follow-up review round is
required.

| Item | Outcome |
| --- | --- |
| F1 | **Closed.** Same-type `==`, `!=`, `<`, `<=`, `>`, `>=` granted; cross-type and cross-origin comparison remain ill-formed; five `::zero()` constants added; unary negation added for the three signed types. |
| E1 | **Applied.** Decision 6's conceptual block is now a three-step derivation naming `sequenceElapsedFor()` and `frameAtSequenceTime()` instead of the forbidden multiply. |
| E2 | **Applied,** and beyond what was asked: rounding toward zero is stated, the helpers are declared not to be exact inverses, and re-anchoring is told to carry the exact anchor rather than round-trip through both helpers. |
| E3 | **Applied.** Criterion 5 now separates the debug precondition from the caller-clamp test. |
| E4 | **Applied.** Criterion 1 is reframed as header review rather than an automated test. |
| E5 | **Applied.** Status is `Accepted`, dated, and the index status column agrees. |

Two changes went further than this review requested, both in the right
direction:

- `FrameRate` gained rational *relational* comparison, not only equality;
- `MasterClockTime` deliberately has **no** `zero()`. A monotonic clock's epoch
  is arbitrary, so a zero master-clock instant would be meaningless. Omitting it
  is correct.

The stale line in [`../decisions/README.md`](../decisions/README.md) was fixed
in the same pass: playback authority is now attributed to ADR-002 rather than
listed as a future decision.

Implementation issues may now depend on ADR-001.

## Reviewer's note

Two positions from earlier rounds are withdrawn by this review, and both are
recorded rather than dropped:

- the round-2 preference for a single shared `Duration` (section 3);
- the round-1 concern that the fixed 30/1 reading of `sourceInFrame` would
  change existing behaviour, already withdrawn in review 2 and confirmed here.
