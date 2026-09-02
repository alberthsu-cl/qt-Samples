# ADR-003 Review 2 — Owner Resolutions

Verdict: **Accept with revisions**

Round: 2 (resolution review)

Scope: the owner's nine proposed resolutions to review 1's B1 and N1–N6. As of
this review the ADR-003 file on disk is unchanged from the version review 1
assessed — the resolutions below are evaluated as stated text, the same way
ADR-001 review 2 and ADR-002 review 2 evaluated proposed wording before it
was committed.

Reviewed against [ADR-001](../decisions/0001-strong-media-time-domains.md) and
[ADR-002](../decisions/0002-playback-session-is-the-state-authority.md) (both
Accepted, closed),
[ADR-003](../decisions/0003-immutable-playback-snapshots-and-generation-gated-presentation.md)
(Proposed),
[review 1](adr-003-review-1-snapshots.md), and
[`../target-playback-architecture.md`](../target-playback-architecture.md).

## Position

Five of the six non-blocking items land cleanly as stated. The blocking
finding, B1, is resolved for two of its three types and left exactly where it
was for the third — `PlaybackMediaDescriptor` is still described by the same
five category nouns as the original text, not by fields. The two domain-safe
variants that were added, `PresentationTarget` and `PresentedPosition`, are
themselves a genuine improvement, but examining them closely surfaces a new
structural finding: `SequencePresentationTarget` carries two independently
settable position descriptors — `ClipId` and `TimelineFrame` — that can
disagree, which is a milder version of the exact anti-pattern this ADR's own
prose warns against ("Parallel source and timeline fields are not used").

Neither problem requires a new design decision. Both are fixable by narrowing
a type or adding one derivation sentence.

## Resolution-by-resolution assessment

### 1. PlaybackMediaDescriptor — not resolved as a type, requirement restated correctly

The resolution restates the same five nouns the original ADR text already
used — "media identity, MediaKind, immutable source locator, availability,
optional source extent, and capabilities" — plus the correct constraint ("no
Qt/MFC objects or mutable library references"), but supplies no field list or
code. Compare this to resolutions 2 and 3, which give exact field
compositions for every variant alternative. `PlaybackMediaDescriptor` is
still the *hardest* of the three original blockers to leave unresolved,
because it is the type embedded directly in `SequencePlaybackSnapshot` — the
ADR's own headline structure — and criterion 2 ("Snapshot code contains no
Qt/MFC objects") cannot be checked against a type that has no declared
fields.

This is not a new problem; it is B1 only three-quarters closed. See the
minimal edit below.

### 2. PresentationTarget — resolved as a domain-safe variant, but introduces B2

```text
SourcePresentationTarget:   MediaAssetId + SourceTimestamp
SequencePresentationTarget: SequenceId + SequenceRevision + TimelineFrame + ClipId
```

The domain split itself is correct and directly answers B1's original
complaint: a source identity can no longer be paired with a sequence
timestamp, or vice versa, because they occupy different variant
alternatives.

`SequencePresentationTarget` is where a new problem appears. It carries two
values that both describe "where in the sequence": `TimelineFrame` (a
position on the frame grid) and `ClipId` (which clip is meant). Nothing
prevents constructing a `SequencePresentationTarget` whose `ClipId` does not
occupy `TimelineFrame` in the referenced `(SequenceId, SequenceRevision)` —
for example, `clipId` from one clip and `timelineFrame` copied from another.
Full analysis under **B2** below.

### 3. PresentedPosition — resolved, and its asymmetry with (2) is informative

```text
PresentedSourcePosition:   MediaAssetId + SourceTimestamp
PresentedSequencePosition: SequenceId + SequenceRevision + TimelineFrame
```

This variant does **not** repeat the problem in resolution 2:
`PresentedSequencePosition` carries only `TimelineFrame`, no `ClipId`. That
is correct, because a *result* position is only ever `TimelineFrame` resolved
against a specific, already-fixed `(SequenceId, SequenceRevision)` — there is
exactly one way to interpret it, and re-resolving `TimelineFrame` against
that snapshot deterministically reproduces which clip was shown. A result
never needs a second, independently-suppliable descriptor for the same
location.

That is exactly why B2 is worth taking seriously: the *result* type shows
what the *request* type should look like. The asymmetry is evidence, not
merely a stylistic observation.

### 4. Natural completion extends, not duplicates, ADR-002 — resolved

The proposed wording — "ADR-003 extends ADR-002's generation invalidation
rules with natural completion" — is exactly the correction N1 asked for. It
keeps the decision (natural completion should invalidate stale work, which is
squarely ADR-003's own question to answer) while fixing the citation (ADR-002's
own list does not include it, so "extends" is accurate and "accepted by"
was not).

### 5. Unavailable-media policy — restated, not closed

"State that unavailable media policy belongs to the media adapter/decoder
contract, while snapshot structure remains valid" adds "decoder contract" to
the original "media adapter" phrase but still names no ADR. This does not
resolve N2, and it leaves a specific promise unfulfilled: ADR-002's own F1
revision states "Detailed media and decoder error taxonomy belongs to ADR-003
**and its decoder contract**" — naming *this* document as the owner of that
decoder contract. A phrase inside ADR-003 pointing at an unnamed "decoder
contract" reads as ADR-003 deferring to itself without saying where in itself,
or to a future document the roadmap has not named. Checking the target
document's own proposed-ADR list (time domains, session authority, snapshots,
audio/clock policy, engine/decoder/thread ownership, sequence identity,
framework boundary) — no entry is titled anything like "decoder error
taxonomy," so there is genuinely nowhere else for this to land yet other than
ADR-005 ("engine, decoder, audio callback, and UI thread ownership"), which is
plausible but not stated.

This does not introduce an *incorrect* deferral — no wrong ADR number is
given — it simply is not yet a deferral to anything checkable. See the
minimal edit below.

### 6. Paused-selection criterion narrowed to the coordinator — resolved

This is exactly what N3 asked for: stop restating ADR-002 criterion 14
verbatim and instead state what this ADR's own layer (the presentation
coordinator) additionally guarantees. No further action needed beyond
applying the narrower wording.

### 7. Project reload added to deterministic tests — resolved

Closes N4 directly. The body already stated the rule ("Reloading a project
creates new runtime sequence identity and revision state"); the criteria list
now has a place to require a test for it.

### 8. New PresentationSessionId on coordinator or project-runtime recreation — resolved, and correctly scoped

"A new `PresentationSessionId` is created when the coordinator or project
runtime is recreated" directly closes N5 and correctly stays inside this
ADR's own scope: it ties presentation-session recreation to *project* reload,
without attempting to also state when `PlaybackSessionId` is recreated —
that boundary belongs to ADR-002, which review 1 correctly declined to reopen,
and this resolution does not reopen it either.

### 9. Stopped and Paused editing preview share one mechanism — resolved

Closes N6 directly: "Stopped editing preview uses the same selection-derived
mechanism as Paused editing preview" removes the wording asymmetry review 1
flagged, without changing either behaviour.

## Status of B1 and N1–N6

| ID | Resolution | Status |
| --- | --- | --- |
| B1 | 1 (descriptor), 2 (target), 3 (position) | **Partially resolved** — target and position variants are domain-safe; the descriptor is still prose |
| N1 | 4 | **Resolved** |
| N2 | 5 | **Not resolved** |
| N3 | 6 | **Resolved** |
| N4 | 7 | **Resolved** |
| N5 | 8 | **Resolved** |
| N6 | 9 | **Resolved** |

## Newly discovered blocker

### B2 — SequencePresentationTarget carries two position descriptors that can disagree

`SequencePresentationTarget { SequenceId; SequenceRevision; TimelineFrame;
ClipId }` gives a caller two independent ways to say "where": a frame-grid
position and a clip identity. The type does not require them to agree, and
nothing else in the ADR states an invariant that would. A coordinator building
an editing-preview request for "clip X's first frame" must, in practice,
already look up clip X's start frame from the snapshot before it can populate
`TimelineFrame` — at which point `ClipId` is redundant information the type
nonetheless allows to be supplied inconsistently.

This matters here specifically because it is a milder recurrence of the exact
problem this ADR's own original text calls out by name: "Parallel source and
timeline fields are not used." That sentence was written to describe the
*source-versus-sequence* split (which resolution 2 has now fixed correctly).
The same reasoning applies one level down, inside the sequence alternative,
between `ClipId` and `TimelineFrame`.

The comparison in resolution 3 makes the fix obvious: `PresentedSequencePosition`
already omits `ClipId` for exactly this reason — a result is unambiguous with
`TimelineFrame` alone. `SequencePresentationTarget` should follow the same
discipline.

**Minimal fix.** Two options, either sufficient; the first requires no new
type:

- Make `ClipId` non-authoritative: state explicitly that when both fields are
  present, `ClipId` is context/diagnostic identity only, and the coordinator
  computes `TimelineFrame` before constructing the request — `ClipId` is never
  used to derive position. One sentence, no structural change.
- Or split the alternative itself: a playhead-following sequence target
  carries only `TimelineFrame`; a clip-selection editing target carries only
  `ClipId` (plus an optional in-clip offset, defaulting to the clip's start).
  This matches the series' established preference for preventing the
  disagreement by construction rather than by stated convention, at the cost
  of one more variant level.

Either is acceptable for milestone 1; the first is smaller.

## Minimal exact edits

| # | Section | Edit | Ref |
| --- | --- | --- | --- |
| 1 | Immutable sequence snapshot | Give `PlaybackMediaDescriptor` an explicit field list in code, matching the rigor already used for `PlaybackClip`: media identity (`MediaAssetId`), `MediaKind`, an immutable source locator, an availability value, `std::optional<SourceTimestamp>` for source extent (absent for still images, mirroring `PlaybackClip::sourceIn`), and a named — even if currently opaque — capabilities type. | B1 |
| 2 | Presentation is separate from transport | Add one sentence resolving B2: either declare `ClipId` non-authoritative for position within `SequencePresentationTarget`, or split the alternative so a request carries `TimelineFrame` or `ClipId`, never both as independent sources of truth. | **B2** |
| 3 | Snapshot construction is an editor-thread transaction (unavailable-media paragraph) | Name where "the decoder contract" lives — either this document's own later section, or ADR-005, given ADR-002's F1 text already assigned "decoder error taxonomy" to "ADR-003 and its decoder contract." | N2 |
| 4 | Session identity and generation (or wherever resolution 4's wording is inserted) | Apply resolution 4's text verbatim; it is correct as proposed. | N1 |
| 5 | Acceptance criteria 9 | Apply resolution 6's narrowing verbatim. | N3 |
| 6 | Acceptance criteria 13 | Apply resolution 7's addition verbatim. | N4 |
| 7 | Session identity / PresentationSessionId lifetime | Apply resolution 8's text verbatim. | N5 |
| 8 | Transport-versus-editing presentation precedence | Apply resolution 9's text verbatim. | N6 |

## Verdict

**Accept with revisions.**

Six of eight items (four N-findings plus two of B1's three types) are closed
by the resolutions as stated, and two of those closures are better than
minimal — resolution 8 correctly scopes presentation-session recreation to
project reload without overreaching into ADR-002's territory, and resolution
4's "extends" framing is exactly the citation discipline the series has been
building toward.

What remains before ADR-003 is implementation-ready: give
`PlaybackMediaDescriptor` real fields (closing B1 completely), add one
sentence resolving the `ClipId`/`TimelineFrame` ambiguity in
`SequencePresentationTarget` (B2), and name where the decoder-error-taxonomy
promise ADR-002 made on this document's behalf is actually kept (N2). None of
the three requires revisiting a decision already made; all three are
definitional.

## Reviewer's note

`PresentedPosition`'s clean shape did double duty in this review: it resolved
N-question 3 on its own terms, and by being simpler than
`PresentationTarget`'s equivalent alternative, it exposed exactly what
`SequencePresentationTarget` should look like. That is worth recording,
because it means the fix for B2 is not a new idea — it is already sitting
next to the problem in the same document.
