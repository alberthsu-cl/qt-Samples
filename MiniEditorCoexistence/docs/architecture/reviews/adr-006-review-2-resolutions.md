# ADR-006 Review 2 — Resolution Verification

Verdict: **Accepted**

Round: 2 (resolution review, doubling as an acceptance check)

Revision reviewed: `8e01009` *Revise ADR-006 after sequence identity review*
(diff confined to ADR-006).

Reviewed against
[ADR-006](../decisions/0006-explicit-sequence-identity-and-project-ready-model.md)
as it stands after `8e01009`, [review 1](adr-006-review-1-sequence-identity.md),
ADR-001 through ADR-005 (all five Accepted, closed), and
[`../target-playback-architecture.md`](../target-playback-architecture.md).

## Verdict

All six findings from review 1 — B1 through B3 and N1 through N3 — are
resolved, and two of the resolutions go further than what was asked: N3 was
offered as optional polish and was resolved with a complete, unambiguous
rule anyway, and the corrected ADR-003 citation (see below) fixes an error
that originated in review 1's own text, not in the ADR.

No new blocking finding. One line is worth a small precision note for a
future pass, not a gate.

## Correction acknowledged: review 1 misattributed the `PresentationSessionId` rule

Before verifying N1, a correction is owed. Review 1's N1 read:

> No explicit link to ADR-004's `PresentationSessionId`-recreation-on-reload
> rule.

That attribution was wrong. The rule — "A new presentation session is created
whenever the coordinator or its project runtime is recreated" — was verified
just now by direct grep against both documents: it appears in
**ADR-003**, not ADR-004. It was added to ADR-003 in that ADR's own round 2
(resolution 8, closing that round's N5), and `PresentationSessionId` itself
is defined only in ADR-003 — it does not appear anywhere in ADR-004's text.

ADR-006's revision cites the rule correctly:

> Project-runtime recreation also creates a new `PresentationSessionId` for
> the preview coordinator, **as defined by ADR-003**.

This is the right citation. The task's request to "check the corrected
ADR-003 reference" was checking exactly this — and the correction needed to
be made was to review 1's own text, which the resolution silently fixed
without perpetuating the error. N1 is resolved with the *correct* source
document, which is a better outcome than resolving it with the citation
review 1 actually gave.

## Resolution status

| ID | Resolution applied | Verified against text |
| --- | --- | --- |
| B1 | `Ready`/`Empty` redefined to be complementary | "`Ready` means an active sequence exists and contains at least one timeline clip… `Empty` means the project loaded successfully but has no active sequence, or its active sequence has zero timeline clips." These now partition every case: `Ready` = active sequence with ≥1 clip; `Empty` = everything else short of `Loading`/`Failed`. No input satisfies both. **Resolved.** |
| B2 | Milestone-1 scope stated | "For milestone 1, loading the existing flat project format synthesizes exactly one default `TimelineSequence` at 30/1. Saving continues to write the existing flat `timelineItems`… Creating or deleting sequences is a target-state capability of this model, not a milestone-1 UI requirement." Matches the target document's own Decision 1 wording closely enough to be clearly the same decision, not a new one. **Resolved.** |
| B3 | `ProjectError` field added | `std::optional<ProjectError> error;` added to `ProjectRuntime`, with "`error` is engaged if and only if `readiness == ProjectReadiness::Failed`" — the exact iff-invariant form `PlaybackStatus::error` already established. **Resolved.** |
| N1 | Cross-reference to the presentation-session rule added | See correction above. **Resolved**, with the citation corrected to ADR-003. |
| N2 | Naming divergence from the target document explained | "`ProjectRuntime` deliberately differs from the target document's `EditorProject` file-shaped sketch. The existing `EditorProject` represents serialized project data; this value represents one loaded runtime. `MediaLibrary` stays outside this struct because playback snapshots contain only descriptors referenced by the active sequence." Both the rename and the dropped field are now explained, and the `MediaLibrary` justification correctly cites ADR-003's own snapshot-scoping rule rather than asserting a new one. **Resolved.** |
| N3 | `PlaybackSessionId` reload behavior stated | "A project reload preserves `PlaybackSessionId` when the existing engine session continues to run… A new `PlaybackSessionId` is created only when the `PlaybackSession` itself is recreated." This was offered as optional in review 1 specifically because correctness did not depend on the answer; the resolution settles it anyway, with the reading review 1 judged most likely. **Resolved**, beyond what was required. |

## New checks for this round

**Contradictions.** None found. The `PlaybackSessionId`-persists /
`PresentationSessionId`-always-recreates split is internally consistent: the
two identities answer different questions (is the same engine session
running, versus is this the same project runtime), and nothing requires them
to change together.

**Type completeness.** `ProjectError` is added as a named, opaque type
consistent with how `PlaybackError` and `PlaybackRejectReason` are already
handled elsewhere in the series — no premature taxonomy, no missing field.

**Milestone-1 fidelity.** The new scope paragraph was checked word-for-word
against the target document's Decision 1 rather than assumed compatible: both
state "one default sequence," "30/1," "saving keeps writing the existing flat
`timelineItems`," and "persisting a sequence frame rate is a
[hard] prerequisite before exposing any other sequence rate." ADR-006 now
carries the same decision the target document already made, not a
paraphrase that could drift from it.

**Scope expansion.** None. Every addition either fixes a definitional gap
identified in review 1 or states a rule already implied elsewhere in the
series (ADR-003's presentation-session rule, ADR-002's implicit
one-long-lived-engine-session assumption).

## N4 — very minor, non-blocking: "can resolve content" leaves one word open to a stricter reading

*Section: Project readiness.*

> `Ready` means an active sequence exists and contains at least one timeline
> clip **from which playback can resolve content**.

B1's overlap is fully closed regardless of how this phrase is read — the
clip-count test alone already makes `Ready` and `Empty` complementary. The
one residual question this phrasing could raise: does "can resolve content"
require the clip's *media* to be currently available (per ADR-003's
`MediaAvailability`), or only that a clip exists at all, independent of
whether its media resolves to real content or a placeholder? ADR-003 already
treats an unavailable-media clip as still producing a snapshot via its
placeholder policy, so the more consistent reading is the latter — a
nonzero clip count, regardless of per-clip media availability. This is
almost certainly the intended meaning and does not need to gate acceptance;
one clause ("regardless of whether every referenced clip's media is
currently available") would remove even the possibility of the stricter
misreading.

## Is ADR-006 implementation-ready?

**Yes.** All six findings from review 1 are closed, the citation error
review 1 introduced has been corrected rather than carried forward, and no
new blocking issue was found. N4 is optional wording polish, the same shape
as the single residual items ADR-004 and ADR-005 closed their own second
rounds with.

## Status change

**ADR-006 may change from Proposed to Accepted as written.** N4 may be
applied in the same pass or left for later; it does not block the status
change.

## Reviewer's note

This round required correcting the reviewer, not just the ADR. Review 1
attributed the `PresentationSessionId`-recreation rule to ADR-004 when it
was ADR-003 that stated it — a citation error a straightforward grep would
have caught at the time, and did catch now, once asked to check it
specifically. The resolution used the correct document without comment,
which is the right way to handle an upstream review's mistake: fix it
quietly, cite it correctly, and let the next verification round confirm
which one moved. Recorded here so it is not lost — review 1's text should be
read with this correction in mind if anyone traces the finding back to its
origin.
