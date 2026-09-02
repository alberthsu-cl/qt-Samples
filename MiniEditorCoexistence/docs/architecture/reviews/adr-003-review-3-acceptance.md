# ADR-003 Review 3 — Final Acceptance Gate

Verdict: **Accepted with revisions**

Round: 3 (final acceptance gate)

Revision reviewed: `6c7b3a8` *Resolve ADR-003 review findings*
(+70 / −16, confined to ADR-003).

Reviewed against
[ADR-003](../decisions/0003-immutable-playback-snapshots-and-generation-gated-presentation.md)
as it stands after `6c7b3a8`, [review 1](adr-003-review-1-snapshots.md),
[review 2](adr-003-review-2-resolutions.md), ADR-001 and ADR-002 (both
Accepted, closed), [`../target-playback-architecture.md`](../target-playback-architecture.md),
and the current codebase (`MediaKind.h`, `MediaLibrary.h`, `TimelineModel.h`).

This is an acceptance gate, not a design pass. No new architecture is
proposed and no closed ADR-001/ADR-002 decision is reopened.

## Verdict

Every finding from rounds 1 and 2 that required a design decision is closed
correctly, and one of them — B2 — is closed *better* than either fix review 2
proposed: rather than making `ClipId` non-authoritative or splitting the
variant further, the revision simply removed `ClipId` from
`SequencePresentationTarget` entirely. Clip identity is now recovered by
resolving `TimelineFrame` against the referenced `(SequenceId,
SequenceRevision)` snapshot, which is exactly how `PresentedPosition`'s
sequence alternative already worked. The two variants are now structurally
identical between request and result, which is the cleanest form this fix
could take.

One defect newly introduced by the same revision blocks acceptance until
corrected, and it is a defect a compiler would catch on the first build, not
a matter of judgment: the code block written to close B1 declares
`enum class MediaKind { Video, StillImage, Audio };` inline, but the codebase
already defines `enum class MediaKind { Video, Audio, Image };` in
`MediaKind.h`, used in sixteen existing files. Two same-named, differently
valued types in one program is a redefinition, not a naming preference. The
fix is one line and requires no new decision: delete the inline declaration
and reference the existing type, exactly as `PlaybackClip` already does for
`TimelineTrackType` two structs earlier in the same document.

## B1 (review 1) — mostly closed; one field's type is wrong

| Requirement | Status |
| --- | --- |
| `PlaybackMediaDescriptor` given real fields | **Closed** |
| Media identity | **Closed** — `MediaAssetId mediaAssetId` |
| Media kind | **Blocking** — redeclares `MediaKind` instead of reusing the existing type; see B3 |
| Immutable source locator | **Closed**, with a minor consistency note — see N7 |
| Availability | **Closed** — `enum class MediaAvailability { Available, Unavailable }`, no collision |
| Optional source extent | **Closed** — `std::optional<SourceTimestamp> sourceExtent`, matching `PlaybackClip::sourceIn`'s absent-for-stills pattern |
| Capabilities | **Closed** — `struct PlaybackCapabilities {}`, an empty, named, extensible placeholder, consistent with how `PlaybackError`/`PlaybackRejectReason` were left opaque in ADR-002 |
| No Qt/MFC objects or mutable library references | **Closed** in principle — every field is a value type or a strong ID |

Five of six components are correctly typed. `MediaKind` is the exception, and
it is the exception specifically because a same-named type already exists in
the codebase this ADR must integrate with — not because the ADR invented
something wrong in isolation.

## B2 (review 2) — closed

`SequencePresentationTarget` is now:

```cpp
struct SequencePresentationTarget {
    SequenceId sequenceId;
    SequenceRevision sequenceRevision;
    TimelineFrame timelineFrame;
};
```

`ClipId` is gone. The accompanying prose states the invariant this depends
on directly: "clip identity is resolved from the referenced `(SequenceId,
SequenceRevision)` snapshot, so parallel position fields cannot disagree."
There is now exactly one way to say "where" in a sequence target, matching
`PresentedSequencePosition`'s shape field-for-field. Review 2's own
observation — that the result type showed what the request type should look
like — is exactly what happened.

Nothing further is needed here.

## N1–N6 (review 1, via review 2) — all closed

| ID | Resolution applied | Verified against text |
| --- | --- | --- |
| N1 | Natural completion is now stated as an extension | "The generation advances exactly at the invalidation boundaries accepted by ADR-002: … ADR-003 extends those generation invalidation rules with natural completion." Citation now matches ADR-002's actual list. |
| N2 | Unavailable-media policy now names an owner | "…the deterministic media-failure/placeholder policy owned by the media adapter and decoder contract defined by ADR-005." Resolved by naming a specific ADR rather than repeating "the media adapter." See the note below on ADR-002 boundary consistency. |
| N3 | Criterion 9 narrowed to the coordinator | "The presentation coordinator's paused clip-selection path changes only presentation request identity …" — no longer restates ADR-002 criterion 14 verbatim. |
| N4 | Project reload added to criterion 13 | "…out-of-order snapshot installs, project reload, viewport clear, and shutdown have deterministic tests." |
| N5 | `PresentationSessionId` recreation trigger stated | "A new presentation session is created whenever the coordinator or its project runtime is recreated." Directly connects project reload to presentation-session identity, which criterion 13's new project-reload test can now exercise meaningfully. |
| N6 | `Stopped`/`Paused` wording aligned | "`Stopped` uses the same selection-derived editing-preview mechanism as `Paused` and may show an idle editing target." |

## Newly discovered blocker

### B3 — `MediaKind` is redeclared with different values than the existing project type

*Section: Immutable sequence snapshot, `PlaybackMediaDescriptor` code block.*

```cpp
enum class MediaKind { Video, StillImage, Audio };
```

`MediaKind.h` already defines:

```cpp
enum class MediaKind {
    Video,
    Audio,
    Image
};
```

used in sixteen existing source files, including the exact media-library and
serialization code this ADR's own migration strategy builds on top of. If
both declarations reach the same translation unit — which they will, since
the snapshot builder must read `LibraryMediaAsset::kind` (the existing
`MediaKind`) to populate `PlaybackMediaDescriptor::mediaKind` — this is an
outright redefinition, not a naming collision that merely reads confusingly.
It will not compile.

The values also differ (`StillImage` versus `Image`) and are in a different
order, so this cannot be read as a harmless duplicate declaration of the same
type; it is a second, incompatible type that happens to share a name.

This is exactly the kind of defect this ADR's own document already shows how
to avoid: two structs earlier, `PlaybackClip.trackType` is typed as the
existing `TimelineTrackType`, reused by reference rather than redeclared.
`MediaKind` should receive the same treatment.

**Minimal fix.** Delete the inline `enum class MediaKind { … };` line and
state that `PlaybackMediaDescriptor::mediaKind` reuses the existing
framework-neutral `MediaKind` already shared by the media library, project
serializer, and timeline edit policy — the same type `LibraryMediaAsset::kind`
already has. No design decision changes; the descriptor keeps every other
field as written.

## Type completeness and framework neutrality

Complete and framework-neutral except for B3. Every field in
`PlaybackMediaDescriptor`, `PresentationTarget`'s two alternatives, and
`PresentedPosition`'s two alternatives is a value type, an `optional`, a
`variant`, or a strong ID — no `QObject`, no MFC handle, no reference into
`MediaLibrary` or `TimelineModel`. `PlaybackCapabilities`'s empty-struct
placeholder is an honest way to defer a detail that genuinely belongs to a
decoder contract without blocking this ADR's own acceptance on it.

## Identity and stale-result rules

Unchanged by this revision and still correct: generation-before-replacement
ordering in `InstallSnapshot`, session/generation/revision matching for
stale-result rejection, and idempotent duplicate observations all read
exactly as round 1 verified them. The only change in this area — the
natural-completion citation fix — corrects an attribution, not a rule, and
the rule itself was already sound.

## Presentation-target consistency

Fully consistent, and this is the strongest part of the revision.
`PresentationTarget` and `PresentedPosition` are now field-for-field
identical between their source alternatives and between their sequence
alternatives respectively. A request and its eventual result describe "where"
the same way, which removes an entire class of translation bugs at the
boundary between them — there is no reshaping required to compare a
`FramePresentationRequest.target` against the `CompositedVideoFrame.position`
it produced.

## ADR-002 boundary consistency

Consistent. Nothing in this revision reopens ADR-002's accepted text or
contradicts a decision it made. The one point worth a forward note rather
than a finding: ADR-002's own F1 revision said "Detailed media and decoder
error taxonomy belongs to ADR-003 and its decoder contract," which read most
naturally as assigning that taxonomy to *this* document. ADR-003 instead
names ADR-005 as the destination. This is a reasonable and permissible
redirection — ADR-002's text does not forbid ADR-003 from forwarding the
detail to whichever future ADR actually owns decoder implementation, and
ADR-005's stated scope ("engine, decoder, audio callback, and UI thread
ownership") is a better fit than ADR-003's own snapshot/generation/
presentation focus. When ADR-005 is drafted, it should acknowledge that this
forwarding chain — ADR-002 to ADR-003 to ADR-005 — is where the promise
finally lands, so a reader tracing it does not have to reconstruct the chain
themselves.

## Acceptance-criteria testability

All fourteen criteria remain objective and testable under MSVC C++17 with
fake clocks and fake media ports; none regressed, and two are now testable
for the first time because the types they depend on finally exist:

- Criterion 5 and criterion 11 depend on `PresentedPosition`'s shape, which
  review 1 flagged as unusable for testing because it did not exist. It now
  does, and both criteria can be written directly against it.
- Criterion 13's project-reload case now has something concrete to assert
  against: after reload, the newly stated `PresentationSessionId` recreation
  rule (N5) gives a test a specific identity change to check for, rather than
  only "something about reload should be deterministic."

Criterion 9's narrower wording is still precise enough to test without
reference to ADR-002's criterion 14, closing the drift risk review 2 flagged.

## Unintended scope expansion or contradictions

None found. `MediaAvailability` and `PlaybackCapabilities` are new but
minimal, single-purpose, and exist only to give `PlaybackMediaDescriptor` a
complete, testable shape — they do not expand what milestone 1 is required to
implement. The ADR-005 redirection narrows a previously vague deferral rather
than expanding ADR-003's own scope. B3 is a defect, not scope creep: it
affects a field's *type*, not what the ADR requires anyone to build.

## Is ADR-003 implementation-ready?

**Yes, after B3.** Every other structural question this gate was asked to
check — snapshot completeness, presentation-target consistency, the
ADR-002 boundary, and acceptance-criteria testability — is answered
correctly by the revision. B3 is one line, requires no new decision, and is
the only thing standing between this document and a clean build of its own
example code.

## Status change

**Recommend flipping ADR-003 from Proposed to Accepted once B3 is applied.**
No further review round should be needed for a one-line type reference fix;
a maintainer may apply it and update the status line and
[`../decisions/README.md`](../decisions/README.md)'s status column in the
same change.

## Reviewer's note

B2's resolution is worth calling out on its own terms, separately from
closing the finding: review 2 offered two fixes, both of which kept `ClipId`
in the type and either annotated it as non-authoritative or split the variant
further to isolate it. The revision took a third option neither review
proposed — deleting the field outright, because nothing in the design
actually needed it once `TimelineFrame` alone is enough to re-resolve the
clip. That is a better answer than either alternative on the table, and it
is worth remembering as a general lesson for this series: when a field can be
recomputed from other fields already present, removing it closes more classes
of bug than documenting its subordinate status ever can.
