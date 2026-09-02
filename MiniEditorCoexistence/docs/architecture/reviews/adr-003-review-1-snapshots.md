# ADR-003 Review 1 — Immutable Playback Snapshots and Generation-Gated Presentation

Verdict: **Accept with revisions**

Round: 1 (original review)

Primary document:
[`../decisions/0003-immutable-playback-snapshots-and-generation-gated-presentation.md`](../decisions/0003-immutable-playback-snapshots-and-generation-gated-presentation.md)
(Status: Proposed)

Reviewed against [ADR-001](../decisions/0001-strong-media-time-domains.md) and
[ADR-002](../decisions/0002-playback-session-is-the-state-authority.md) (both
Accepted, closed), [`../target-playback-architecture.md`](../target-playback-architecture.md),
[`../current-playback-architecture.md`](../current-playback-architecture.md),
[`../../../README.md`](../../../README.md), all ADR-001 and ADR-002 reviews,
and the current code: `TimelineModel`, `TimelinePlaybackResolver`,
`TimelineEditingController`, `ProjectState.h`, `MediaLibrary`.

## Verdict

This is the strongest first-round draft in the series. It correctly applies
ADR-001's type discipline to a concrete data structure for the first time
(`PlaybackClip` uses `TimelineFrame`, `FrameCount`, and
`std::optional<SourceTimestamp>` exactly as ADR-001 requires), it resolves the
fourth question it sets for itself — non-transport clip-selection presentation
— with a precedence policy detailed enough to protect the exact product
behaviour ADR-002's B10 was written to save, and its deferrals to ADR-004 and
ADR-005 are correctly placed everywhere they are made.

One structural gap blocks acceptance: three types load-bearing enough to
appear in the ADR's two headline structures — `PlaybackMediaDescriptor`,
`PresentationTarget`, and `PresentedPosition` — are described only in prose,
while every other type in the document is given as C++. This is not a
stylistic inconsistency. `PresentationTarget`'s prose promise, "Parallel
source and timeline fields are not used," is exactly the anti-pattern
ADR-002 needed three review rounds to eliminate from `PlaybackStatus` — by
making it unrepresentable with a variant, not by asserting it in comments.
Leaving the equivalent type unspecified here re-opens the possibility the
series has already spent real effort closing.

The fix is definitional, not a new design decision, and several non-blocking
items would sharpen the document further without touching its scope.

## Findings

| ID | Finding | Classification |
| --- | --- | --- |
| B1 | Three central types left in prose only | **Blocking** |
| N1 | Natural completion misattributed to ADR-002's generation list | Non-blocking |
| N2 | Media-failure/placeholder policy has no named owner | Non-blocking |
| N3 | Acceptance criterion 9 duplicates ADR-002 criterion 14 verbatim | Non-blocking |
| N4 | Criterion 13's scenario list omits project reload | Non-blocking |
| N5 | `PresentationSessionId` recreation trigger is unstated | Non-blocking |
| N6 | "Stopped may show an idle editing target" is looser than the Paused rule it mirrors | Non-blocking |
| — | Domain-safe use of `TimelineFrame` / `FrameCount` / `SourceTimestamp` / `FrameRate` | Resolved by ADR-001 |
| — | `PlaybackPhase`, `Stopped`/`Paused` semantics, generation/session identity mechanics | Resolved by ADR-002 |
| — | Clock advancement, rate conversion, re-anchoring | Correctly deferred to ADR-004 |
| — | Command-queue ownership, thread join, shutdown mechanics | Correctly deferred to ADR-005 |

## 1. Snapshot completeness and immutability

**Immutability is sound.** `SequencePlaybackSnapshotPtr = std::shared_ptr<const
SequencePlaybackSnapshot>` plus the stated rule that "the `const` pointee is
part of the contract" and building never holds an engine lock while installing
never reads editor state — this is a clean transaction boundary, and it
correctly identifies `SequencePlaybackSnapshotPtr` as the *only* value crossing
it.

**Completeness cannot be fully verified**, because `PlaybackMediaDescriptor` —
the type that carries "media kind, immutable source locator, optional source
extent, availability, and capabilities" for every asset the snapshot
references — has no declared shape. A reader cannot check whether the
descriptor carries enough identity to be looked up from a `PlaybackClip`'s
`mediaAssetId`, nor whether "no Qt type" (which a `SnapshotBuildError` and
criterion 2 both depend on) actually holds. See B1.

Verified against code: `contentDurationFrames()` already returns the greatest
clip end and zero for an empty model — exactly the rule the ADR states for
snapshot duration — and is already the value used to drive real playback
completion (`TimelineEditingController::synchronizePlaybackDuration`), distinct
from `durationFrames()`'s 600-frame UI-canvas floor. The ADR's duration rule
needed no correction against the codebase.

## 2. SequenceRevision rules, including undo/redo and unrelated media imports

**Correct and directly answers both named cases.** Undo and redo are
explicitly required to "create a new revision rather than reusing its
historical number," which is the right call — a revision is runtime identity
for asynchronous work, not a content hash, so two installs of textually
identical content must still invalidate each other's in-flight work.

Unrelated media imports are handled by construction rather than by a rule that
could be gotten wrong: "The snapshot contains descriptors only for assets
referenced by its clips. Importing, removing, or changing an unreferenced
library asset therefore does not change the sequence revision." Removing a
*referenced* asset is not a case this ADR needs to handle, because the existing
edit layer already refuses that removal (README: "Remove prevents an asset
from being deleted while a timeline placement still references it").

View-only exclusions (selection, playhead, panel visibility, zoom) match the
existing `TimelineViewState` / `TimelineAudioMixState` split in
`ProjectState.h`, where the audio-mix state's own comment already says "unlike
`TimelineViewState`, this changes the produced mix and must therefore be
saved" — the ADR's revision boundary lands exactly on a distinction the code
already draws.

## 3. Snapshot validation and missing-media behavior

The validation list is concrete and each item maps to an existing invariant
(`TimelineTrackPolicy` non-overlap, `ClipFade`/DSP clamp bounds, positive
duration, nonnegative start). The two-tier failure model is right: a
referenced descriptor that resolves to a missing file becomes an *unavailable*
descriptor (not a build failure), while a clip with no descriptor at all *is* a
build failure. That distinction matters because the first is a normal runtime
condition a project can be saved and reopened in, and the second indicates
snapshot-builder/editor-state disagreement.

What "the deterministic media-failure/placeholder policy owned by the media
adapter" actually *is* remains unstated, and no future ADR is named for it —
see N2. This does not block acceptance because it does not create a
contradiction or an untestable criterion; it is a documentation completeness
gap, not a design gap.

## 4. PlaybackGeneration invalidation and stale-result rejection

The mechanism is sound: generation is advanced before any replacement work is
scheduled (`InstallSnapshot` step 2, before the snapshot pointer itself is
replaced in step 3), a mismatch on session, generation, or — for sequence
work — sequence/revision is stale, and a stale result "performs no state
transition, UI publication, renderer update, or error fallback." Cooperative
cancellation being explicitly a performance optimization only, not a
correctness dependency, is the right stance and matches ADR-002's identical
position.

One citation is inaccurate. The ADR states generation "advances exactly at the
invalidation boundaries accepted by ADR-002: every seek, source replacement,
snapshot replacement, stop/shutdown that invalidates work, **and natural
completion**." ADR-002's own list — "every accepted seek … playback-source
replacement; sequence-snapshot replacement; stop/shutdown when pending work
must be discarded" — does not include natural completion. This is not wrong as
a decision: natural completion transitioning through an implicit
Stop-equivalent plausibly should invalidate stale decode work, and ADR-003's
stated question 3 ("How are late decode, composition, and frame-publication
results rejected?") is squarely this ADR's own territory to answer. The
citation should say ADR-003 *extends* ADR-002's boundary set, not that ADR-002
already established this one. See N1.

## 5. Domain-safety of request/result identity variants

`PlaybackWorkIdentity` and `SequenceWorkIdentity` are domain-safe: the latter
correctly layers `SequenceId` + `SequenceRevision` on top of the former for
sequence-scoped work, and the ADR explicitly notes audio work carries the same
identity even though ADR-004 owns its buffering policy — the identity/policy
split is drawn in the right place.

`TransportPresentationIdentity { PlaybackSessionId; PlaybackGeneration }`
omitting `SequenceRevision` is *not* a gap: because every `InstallSnapshot`
already advances generation (per finding 4), generation alone is a strict
superset watermark of revision changes for transport purposes. Adding revision
here would be redundant, not more correct.

`PresentationTarget` and `PresentedPosition`, however, cannot be assessed for
domain-safety at all — they are the two types this question most needs to
examine, and both are prose-only. See B1.

## 6. PresentationSessionId and PresentationRequestId lifetime rules

The rules given are correct as far as they go: request IDs are scoped to one
presentation session and increment on every desired-target change including
scrubbing, and "a result from an older presentation session is stale even if
its numeric request ID equals one in the new session" is exactly the right
invariant for a two-level identity scheme.

What is missing is *when* a new `PresentationSessionId` is minted. The only
stated trigger is "coordinator lifetime," and milestone 1 has one long-lived
preview viewport — so in practice this identity may never change during a
session, which makes the stale-numeric-collision rule currently untestable in
milestone 1's own scope. The ADR separately states that "Reloading a project
creates new runtime sequence identity and revision state," which is exactly
the kind of event that plausibly *should* also recreate the presentation
session, but the two statements are never connected. See N5.

## 7. Paused clip-selection presentation without transport mutation

Correct and complete, and it directly protects the product behaviour at risk.
The precedence rule — "`Paused` keeps transport frozen, but explicit
timeline-clip selection may temporarily show an editing target without moving
the playhead or changing phase" — is paired with an explicit statement of
*why*: "This preserves the editor's Split workflow: selecting a clip does not
seek the timeline merely to obtain its first frame." That sentence is a direct
citation of the same README passage ADR-002's B10 finding was built on
("Selecting a clip no longer resets the head, so the same ruler position can
immediately enable Split"), and acceptance criterion 9 tests exactly this.

## 8. Transport-versus-editing presentation precedence

The six-case policy is thorough and each transition out of an override is
named ("the next `Play`, `Seek`, `Stop`, `OpenSource`, or `InstallSnapshot`
clears that editing override"). `Failed` retaining the last accepted frame
while showing an error, and an explicit clear being required rather than
absence-of-new-frame implicitly clearing, are both correct and match the
"latest-wins, never silently blank" spirit of the rest of the document.

One wording asymmetry: `Paused`'s override is gated on "explicit timeline-clip
selection," but `Stopped`'s parallel case says only "`Stopped` may show an idle
editing target" without naming the same trigger. The intended reading is
almost certainly identical, but as written it could be misread as
implementation-defined default behaviour rather than the same
selection-derived mechanism. See N6.

## 9. Atomic frame publication and FramePresented acknowledgement

Correct, and it resolves a genuine ambiguity ADR-002's review left open
cleanly by design rather than by afterthought: preroll completion is
satisfied by decode/composition readiness (`CompositedVideoFrame`), never by
`FramePresented`, so the engine's state machine never depends on GPU or
widget commit timing. `FramePresented` "updates presentation diagnostics
only" and explicitly does not advance the master clock — consistent with
ADR-002's `Paused` definition, "it does not claim that a renderer has already
presented the requested frame," which this ADR now fulfils rather than
merely cross-references.

The atomicity claim itself — "An accepted frame and its position metadata are
published to the UI as one immutable value" — is sound in principle but,
again, cannot be fully checked without `CompositedVideoFrame::position`'s
(`PresentedPosition`'s) actual shape. See B1.

## 10. Queue bounds and implementability of latest-wins behavior

Implementable under MSVC C++17 with an in-flight slot plus one pending slot
per stream — no lock-free structure or custom allocator is required, and the
document is explicit that the bound applies to *video* work, correctly
leaving audio's continuous buffering to ADR-004 rather than forcing it
through the same single-frame model. "Replacing pending work releases its
snapshot and frame resources immediately" is the right guarantee for the
aggressively bounded queue depth chosen, and the milestone's own stated cost
— "may sacrifice throughput for deterministic responsiveness" — is an honest
trade-off rather than a hidden one.

## 11. C++17 feasibility and consistency with ADR-001/ADR-002

Fully feasible: every construct shown (`std::variant`, `std::optional`,
`std::shared_ptr<const T>`, at-least-64-bit monotonic counters with explicit
non-wrap/non-reuse and fail-rather-than-reuse-on-exhaustion semantics) is
plain C++17, and the exhaustion-handling language is lifted consistently from
ADR-002's identical treatment of `PlaybackGeneration`.

Consistency with ADR-001 is the strongest part of the document: `PlaybackClip`
is the first concrete data structure in the series to apply ADR-001's
position/duration/instant separation without a single domain slip —
`TimelineFrame startFrame`, `FrameCount duration`,
`std::optional<SourceTimestamp> sourceIn` are all exactly the types ADR-001
would require, including the `optional` for still images that ADR-001's own
N2 asked for.

Consistency with ADR-002 holds throughout: `InstallSnapshot`'s six-step
sequence elaborates ADR-002's row without changing its semantics, and phase
names and their meanings (`Stopped` "positioned at its defined start," etc.)
are reused rather than redefined.

## 12. Deferral boundaries to ADR-004 and ADR-005

Both are drawn correctly everywhere they are stated: "Audio buffer depth,
latency, underflow, and clock policy belong to ADR-004," "ADR-004 defines
scheduling deadlines and A/V policy," and "Command-queue ownership and
shutdown mechanics belong to ADR-005" / "Thread joining and destruction
belong to ADR-005" all match the deferral language already established by
ADR-001 and ADR-002.

One deferral has no destination: "the deterministic media-failure/placeholder
policy owned by the media adapter" names an implementation component, not an
architectural decision or a future ADR. This also leaves an explicit promise
from ADR-002 unfulfilled — ADR-002's F1 revision states "Detailed media and
decoder error taxonomy belongs to ADR-003 and its decoder contract" — and
ADR-003 never defines `PlaybackError`, a decoder error taxonomy, or connects
`SnapshotBuildError` to either. See N2.

## 13. Are the acceptance criteria objective and testable?

Twelve of the fourteen criteria are directly testable under MSVC C++17 with
fake clocks and fake media ports, and several are stronger than equivalent
criteria earlier in the series — criterion 2 ("Snapshot code contains no
Qt/MFC objects") is enforceable structurally, the same way
`MiniEditorCoreTests` already links no Qt today, rather than by inspection
alone.

Two are affected by findings above rather than being newly broken themselves:

- Criterion 5 ("Every asynchronous request/result carries the required …
  identity") and criterion 11 ("Frame pixels and position metadata are
  accepted and published atomically") both depend on `PresentedPosition`'s
  shape, which does not yet exist to test against. Resolved once B1 lands.
- Criterion 13's scenario list is otherwise thorough but omits the project-
  reload case the body itself states as a rule. See N4.

Criterion 9 is functionally identical to ADR-002's criterion 14; keeping both
is not wrong, but restating rather than cross-referencing risks the two
drifting apart if either is revised later. See N3.

## Non-blocking improvements

**N1 — Citation accuracy for natural completion.** Reword "the invalidation
boundaries accepted by ADR-002 … and natural completion" to state that
ADR-003 extends ADR-002's boundary set with natural completion, since
ADR-002's own list does not include it.

**N2 — Name an owner for the media-failure/placeholder policy**, or state
explicitly that it is intentionally left as an implementation detail
requiring no architectural decision (analogous to codec support), rather
than an undischarged deferral.

**N3 — Cross-reference rather than duplicate ADR-002 criterion 14.** ADR-003
criterion 9 restates it verbatim; point to it instead, or state explicitly
that this criterion additionally covers the presentation-coordinator layer.

**N4 — Add project reload / new sequence identity to criterion 13's scenario
list**, since the body states it as a rule ("Reloading a project creates new
runtime sequence identity and revision state") that the criteria do not
currently require a test for.

**N5 — State what recreates a `PresentationSessionId`.** At minimum, whether
project reload recreates it alongside the sequence identity, even if
milestone 1 never exercises any other trigger.

**N6 — Align the `Stopped` and `Paused` override wording.** State that
`Stopped`'s "idle editing target" comes from the same selection-derived
mechanism as `Paused`'s override, not an unspecified default.

## Reviewer's note

This draft shows the series' accumulated lessons being applied prospectively
rather than only in response to review: the natural-completion generation
rule, the Failed-frame-retention policy, and the FramePresented/preroll
separation all pre-empt questions earlier rounds had to ask about ADR-001 and
ADR-002. The one place that lesson was not carried all the way through is the
one that matters most — making the "no parallel domain fields" promise a
type rather than a comment, for the two structures whose entire job is
presenting a frame without confusing which domain it came from.
