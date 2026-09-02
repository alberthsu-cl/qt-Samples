# ADR-006 Review 1 — Explicit Sequence Identity and Project-Ready Model

Verdict: **Accept with revisions**

Round: 1 (original review)

Primary document:
[`../decisions/0006-explicit-sequence-identity-and-project-ready-model.md`](../decisions/0006-explicit-sequence-identity-and-project-ready-model.md)
(Status: Proposed)

Reviewed against ADR-001 through ADR-005 (all five Accepted, closed) and
[`../target-playback-architecture.md`](../target-playback-architecture.md).

## Verdict

The core shape is right and scales the way it should: `ProjectRuntime`'s
`std::vector<TimelineSequence>` plus `std::optional<SequenceId>
activeSequenceId` is a design that adding a second sequence extends rather
than restructures, and the explicit-selection rule ("Playback never infers
its sequence from UI focus…") is the correct continuation of ADR-002's
already-established anti-focus-inference principle into project/sequence
selection specifically.

Three issues block acceptance. Two are definitional gaps of the kind this
series has repeatedly needed to close before implementation can begin safely
— a state-machine ambiguity in the four readiness values, and a promised
error field that the shown struct does not actually have. The third is an
omission this ADR is the one document positioned to notice: the target
document it formalizes already states the exact milestone-1 constraint this
ADR silently drops.

None require a new design decision. Three further non-blocking items would
sharpen cross-ADR traceability without changing anything already decided.

## Findings

| ID | Finding | Classification |
| --- | --- | --- |
| B1 | `Ready` and `Empty` definitions overlap for "active sequence, zero clips" | **Blocking** |
| B2 | No milestone-1 scope statement; drops a constraint the target document already states explicitly | **Blocking** |
| B3 | `ProjectRuntime` has no field for the error `Failed` readiness promises | **Blocking** |
| N1 | No explicit link to ADR-004's `PresentationSessionId`-recreation-on-reload rule | Non-blocking |
| N2 | Unexplained rename from the target document's `EditorProject` to `ProjectRuntime`, and a dropped `MediaLibrary` field | Non-blocking |
| N3 | Whether project reload creates a new `PlaybackSessionId` is left open | Non-blocking — see reasoning below |
| — | Reused `SequenceId` across different project loads is safe under the existing generation-based identity layers | Verified, resolved by ADR-002/003 |
| — | `TimelineModel` reused by reference, not redeclared | Consistent with ADR-003's established practice |
| — | Multi-sequence scalability of the data shape itself | Confirmed sound |

## B1 — `Ready` and `Empty` overlap for an active sequence with no clips

*Section: Project readiness.*

The two definitions, read independently, both claim the same state:

> - `Ready` means at least one valid sequence **can produce a snapshot**.
> - `Empty` means the project loaded successfully but has **no active
>   sequence or no timeline clips**. It is valid state, not an error.

An active sequence with zero clips satisfies both. ADR-003 already
establishes that "an empty snapshot is valid" — a sequence with no clips
*can* produce a snapshot, which is exactly `Ready`'s stated test. But
`Empty`'s definition explicitly includes "no timeline clips" as one of its
two qualifying conditions, with no active-sequence requirement attached to
it. As written, there is no single input that distinguishes which of the two
values applies to "one active sequence, zero clips" — a case milestone 1 will
reach immediately, since a freshly synthesized sequence starts exactly there.

This is not a hypothetical edge case; it is the *starting* state of every
project this milestone opens. Acceptance criterion 3 requires "loading,
ready, empty, and failed project states are distinct and testable" — as
currently defined, they are not distinct at this boundary, which makes
criterion 3 unsatisfiable for that input without a clarifying rule.

**Minimal fix.** State the tie-break explicitly. Either: "`Ready` requires
the active sequence to have at least one clip; an active sequence with zero
clips is `Empty`," or the reverse: "`Empty` applies only when no sequence is
active; an active sequence with zero clips is `Ready` and produces an empty
snapshot." Both are internally consistent with the rest of the document —
the later "Snapshot and playback boundary" section's own careful split
between "no active sequence" (no snapshot at all) and "active sequence,
possibly empty" (produces the empty-snapshot case) reads slightly more
naturally with the second option, but either resolves the ambiguity equally
well. One sentence, no new decision.

## B2 — No milestone-1 scope statement, dropping a constraint the target document already states

*Whole document.*

"Milestone 1" does not appear anywhere in ADR-006 — the only document in the
series of six not to say it. Every sibling ADR states its milestone-1 scope
explicitly and separately from its target-state design (ADR-001's synthesized
30/1 sequence, ADR-002's steady-clock-only exception worded identically,
ADR-004's audio/monotonic exception, ADR-005's single-session note).

This matters here specifically because the target document Decision 1 —
which ADR-006 exists to formalize — already made this exact statement about
this exact struct:

> For milestone 1 this structure is an in-memory model only. Loading the
> existing flat project format synthesizes one default sequence at `30/1`,
> and saving keeps writing the existing flat `timelineItems`. Persisting a
> sequence `FrameRate` is a hard precondition before the product exposes any
> other sequence rate.

ADR-006's own version of `ProjectRuntime` carries a `std::vector<TimelineSequence>`
and lifecycle rule 3 states generally that "Creating or deleting a sequence
changes project structure and selects an active sequence explicitly" — with
nothing anchoring this to "milestone 1 has exactly one synthesized sequence,
created automatically at load, with no UI to add another." A reader cannot
tell from ADR-006 alone whether sequence creation/deletion is something to
build now or a target-state capability the data shape merely needs to permit
later — the same distinction every other ADR in this series draws for its
own decision.

**Minimal fix.** One paragraph carrying forward the target document's own
milestone-1 sentence: state that milestone 1 synthesizes exactly one
`TimelineSequence` per loaded project (matching ADR-001's already-Accepted
scope), that the project format does not change, and that "creating or
deleting a sequence" describes the target-state contract the data shape must
support, not a milestone-1 UI requirement.

## B3 — `ProjectRuntime` has no field for the `Failed` error

*Section: Decision, `ProjectRuntime` struct; Project readiness.*

> `Failed` means project loading or validation failed and carries a
> **framework-neutral project error outside the sequence value**.

The struct shown is:

```cpp
struct ProjectRuntime {
    ProjectId projectId;
    std::vector<TimelineSequence> sequences;
    std::optional<SequenceId> activeSequenceId;
    ProjectReadiness readiness;
};
```

There is no field to hold the promised error. "Outside the sequence value"
correctly rules out putting it inside `TimelineSequence`, but nothing takes
its place on `ProjectRuntime` either. This is the same class of gap ADR-002's
original `PlaybackStatus` and ADR-003's original `PlaybackMediaDescriptor`
both had before their own review rounds closed it, and this series already
has the exact pattern to reuse: `PlaybackStatus` carries
`std::optional<PlaybackError> error`, engaged if and only if
`phase == Failed`.

**Minimal fix.** Add `std::optional<ProjectError> error;` (or an
equivalently named field) to `ProjectRuntime`, engaged if and only if
`readiness == ProjectReadiness::Failed` — stated as an explicit invariant,
matching `PlaybackStatus`'s. The error type itself may stay opaque at this
architectural level, exactly as `PlaybackError` and `PlaybackRejectReason`
have throughout the series.

## ProjectId and SequenceId lifecycle rules

Rules 1, 2, 4, 5, and 6 are sound and each is independently testable. Rule 2
in particular — "a project reload never reuses the previous runtime
`SequenceId`, even if the file contents are unchanged" — is exactly ADR-003's
already-Accepted rule ("Reloading a project creates new runtime sequence
identity and revision state"), correctly restated rather than reinterpreted.

`SequenceId` scoping is deliberately narrow — "unique for the lifetime of one
loaded project runtime" — which permits the *same* numeric ID to recur across
two different project loads. This was checked carefully rather than assumed
safe: a stale `SequenceWorkIdentity` from a closed project cannot be mistaken
for current work in a newly reopened one even if its `SequenceId` value
happens to repeat, because `SequenceWorkIdentity` also carries
`PlaybackWorkIdentity` (`PlaybackSessionId` + `PlaybackGeneration`), and every
project close or reload is a snapshot/source replacement that unconditionally
advances generation under ADR-002/ADR-003's already-Accepted rules. The
narrow per-runtime scoping is therefore the right amount of guarantee, not an
under-specification — matching this series' consistent preference (see
ADR-001's N7) for not manufacturing a stronger guarantee than the milestone
needs.

Rule 3 is the one place needing B2's scoping statement to be unambiguous, as
discussed above.

## Project reload and stale-work invalidation

Correct, and safe by construction rather than merely by rule. Rule 6's
"invalidates playback work through ADR-002/ADR-003" is really a correctness
belt on top of a design that does not need it for memory safety:
`SequencePlaybackSnapshotPtr` is `std::shared_ptr<const
SequencePlaybackSnapshot>`, a self-contained immutable value with no live
reference back into `TimelineModel`, `MediaLibrary`, or `ProjectRuntime` —
exactly ADR-003's point. Destroying the old `ProjectRuntime` on close cannot
dangle a snapshot the engine still holds; rule 6's actual job is preventing
*new* work from being scheduled against the closing project, which the
generation-advance mechanism already handles.

## Loading/Ready/Empty/Failed semantics

`Loading` and `Failed` are each clearly and singly defined. `Ready` and
`Empty` overlap; see B1. Once B1 is resolved, all four states are
individually well-motivated: treating `Empty` as "valid state, not an error"
is the right call, and it correctly keeps an empty project from being
special-cased as a failure the way an ad hoc implementation might default to.

## Source-preview versus sequence-preview boundaries

Correct and consistent with ADR-002's `PlaybackSource` variant. "Selecting a
library asset creates a `SourceAssetPreview` request, not a `SequencePreview`
request" and "it must not silently play the first library asset" are both
direct, correctly-scoped restatements of already-Accepted behavior — the
second is the same invariant ADR-002's B10/ADR-003's review series worked to
protect (selection is not transport intent) applied to the empty-project
case specifically.

## Snapshot construction and active-sequence selection

Consistent with ADR-003. "The project/editor thread builds a
`SequencePlaybackSnapshot` only from one completed, active `TimelineSequence`"
matches ADR-003's "the UI/editor thread builds a candidate entirely from one
completed editor state" exactly, narrowed correctly to name *which* editor
state (the active sequence, not any sequence, not the whole project).

## Compatibility with ADR-002 and ADR-003

No contradiction found beyond B1–B3 above. `TimelineSequence.timeline` reuses
the existing `TimelineModel` type by reference rather than redeclaring it —
correctly following the lesson ADR-003's own `MediaKind` collision taught the
series, not repeating it. The generation/revision layering discussed above
holds up under direct scrutiny, not just by citation.

## Multiple-sequence scalability

The data shape itself scales cleanly: a second sequence is an append to
`sequences`, switching context is reassigning `activeSequenceId`, and
`SequenceRevision` already tracks per-`SequenceId` per ADR-001/003, so
concurrent independent revision lines for two sequences need no new
mechanism. The only scalability question this round found is not about the
shape but about the document's own clarity — B2 — since the data model
supporting multiple sequences and milestone 1 *exposing* multiple sequences
are two different claims this document currently conflates by not
distinguishing them.

## Acceptance-criteria testability

Eight of nine criteria are objective and testable under MSVC C++17 with no
Qt/MFC/codec/hardware dependency, matching the pattern this series has
established (criterion 9 requires exactly that, and the codebase already has
a zero-Qt-linked test target to enforce it structurally).

Criterion 3 — "Loading, ready, empty, and failed project states are distinct
and testable" — is not currently satisfiable at the boundary B1 identifies,
since two of the four states are not distinct there. It becomes fully
testable once B1 is resolved; no other criterion is affected.

## Missing invariants, contradictions, or scope expansion

Two missing invariants (B1's tie-break, B3's error field) and one scope
clarity gap (B2) that, left unresolved, would allow a reader to believe
milestone 1 requires more than ADR-001 already decided it does. No
unrelated scope expansion: every field and rule in this document serves the
sequence-identity/project-readiness question it sets out to answer.

## Non-blocking improvements

**N1 — Link project-runtime recreation to ADR-004's presentation-session
rule.** ADR-004 states "a new presentation session is created whenever the
coordinator or its **project runtime is recreated**" — language that
anticipates this exact document. ADR-006 never closes the loop by name. One
cross-reference sentence would let a reader trace the connection instead of
noticing the phrase repeats by coincidence.

**N2 — Explain the departure from the target document's sketch.** The target
document's Decision 1 names this concept `EditorProject`; ADR-006 calls it
`ProjectRuntime` and drops the target sketch's `MediaLibrary` field. The
rename is very likely deliberate and correct — the codebase already has an
`EditorProject` type with an entirely different, project-*file*-shaped
layout (`mediaAssets`, `clipSettings`, `timelineClips`, `timelineItems`), and
reusing that name for this differently-shaped runtime value would repeat the
exact naming collision ADR-003's `MediaKind` review round had to fix. One
sentence stating this explicitly would turn a silent, easily-second-guessed
divergence into a documented decision.

**N3 — Whether project reload creates a new `PlaybackSessionId` is left
open, but this does not affect correctness.** ADR-002 never states when a
`PlaybackSessionId` changes, and ADR-006 — the first document to give
"project reload" a formal identity and lifecycle — does not settle it
either. This is downgraded from a blocking finding because the property that
actually matters, stale-work rejection, does not depend on the answer: every
project close or reload is a snapshot/source replacement, which
unconditionally advances `PlaybackGeneration` under ADR-002/ADR-003 regardless
of whether the surrounding `PlaybackSession` object and its ID persist across
the reload or are recreated. Settling the question would sharpen the
document's own lifecycle story without changing any test's expected result,
which is why it is offered as an optional improvement rather than a
condition for acceptance.

## Minimal exact edits

| # | Section | Edit | Ref |
| --- | --- | --- | --- |
| 1 | Project readiness | State the `Ready`/`Empty` tie-break for an active sequence with zero clips. | **B1** |
| 2 | Decision, or a new paragraph immediately after it | State milestone 1's scope: one synthesized sequence per loaded project, unchanged project format, "creating or deleting a sequence" as target-state contract. | **B2** |
| 3 | Decision, `ProjectRuntime` struct; Project readiness | Add `std::optional<ProjectError> error` to `ProjectRuntime`, engaged if and only if `readiness == Failed`. | **B3** |
| 4 | Identity and lifecycle rules, or Snapshot and playback boundary | Cross-reference ADR-004's presentation-session-recreation-on-project-runtime-recreation rule by name. | N1 |
| 5 | Decision | State why this document names the value `ProjectRuntime` rather than the target document's `EditorProject`, and why `MediaLibrary` is out of this struct's scope. | N2 |
| 6 | Identity and lifecycle rules | Optionally state whether `PlaybackSessionId` persists or is recreated across a project reload. | N3 |

## Reviewer's note

Two of this round's three blocking findings — B1 and B3 — are the same
shape of gap this series has now seen close cleanly in every prior ADR:
a state enum whose values are not yet provably distinct, and a struct
missing the field its own prose promises. Both have a one-sentence or
one-field fix already modeled elsewhere in this document set. B2 is the more
interesting one: it is the first case in the series where the *target*
document already had the answer, and the ADR meant to formalize that
document's decision simply did not carry the sentence over.
