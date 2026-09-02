# ADR Reviews

Independent architecture reviews of proposed and accepted ADRs. A review is a
record of what was questioned and how it was resolved; it is not itself a
project contract. Contracts live in
[`../decisions/`](../decisions/README.md).

Each review states one verdict: **Accept**, **Accept with revisions**, or
**Reject**. Findings are numbered so later rounds and implementation issues can
cite them:

- `B<n>` — blocking. Must be resolved before implementation depends on the ADR.
- `N<n>` — non-blocking improvement.
- `RB<n>` — blocking issue still open after a resolution round.
- `F<n>` — finding from a final acceptance gate.

## Index

| Review | Subject | Round | Verdict |
| --- | --- | --- | --- |
| [ADR-001 review 1](adr-001-review-1-time-domains.md) | Strong Media Time Domains | 1 — original | Accept with revisions |
| [ADR-001 review 2](adr-001-review-2-resolutions.md) | Owner resolutions to review 1 | 2 — resolutions | 4 of 5 blockers closed |
| [ADR-001 review 3](adr-001-review-3-final.md) | Final acceptance gate | 3 — final | Accept with minor editorial changes — closed |
| [ADR-002 review 1](adr-002-review-1-playback-authority.md) | PlaybackSession is the playback-state authority | 1 — original | Accept with revisions |
| [ADR-002 review 2](adr-002-review-2-resolutions.md) | Owner resolutions to review 1 | 2 — resolutions | Accept with blocking revisions |
| [ADR-002 review 3](adr-002-review-3-final.md) | Final acceptance gate | 3 — final | Accept with minor editorial changes — closed |
| [ADR-003 review 1](adr-003-review-1-snapshots.md) | Immutable playback snapshots and generation-gated presentation | 1 — original | Accept with revisions |
| [ADR-003 review 2](adr-003-review-2-resolutions.md) | Owner resolutions to review 1 | 2 — resolutions | Accept with revisions |
| [ADR-003 review 3](adr-003-review-3-acceptance.md) | Final acceptance gate | 3 — final | Accepted with revisions |

## Reading order for ADR-003

Read review 1 for `B1` and `N1`-`N6`, review 2 for the owner's resolutions
and `B2`, then review 3 for the acceptance gate on revision `6c7b3a8`.

Review 2 closed N1 and N3 through N6, and closed two of B1's three types:
`PresentationTarget` and `PresentedPosition` became domain-safe variants, but
`PlaybackMediaDescriptor` was still prose rather than fields. Examining the
new `PresentationTarget` variant also surfaced `B2`: its sequence alternative
carried both `ClipId` and `TimelineFrame`, two position descriptors nothing
prevented from disagreeing.

Review 3 confirms B1 and B2 both resolve in the applied revision — B2 better
than either fix review 2 proposed, by dropping `ClipId` entirely rather than
annotating or splitting around it — and that N1 through N6 all close. It
raises one new blocking finding, `B3`: the code closing B1 redeclares
`MediaKind` with different values than the type already defined in
`MediaKind.h` and used in sixteen existing files, which will not compile once
both reach the same translation unit. The fix is one line and needs no new
decision. ADR-003 remains Proposed pending it.

## Reading order for ADR-002

Read review 1 for findings `B1` through `B8` and the reasoning behind each,
then review 2 for the owner's resolutions.

Review 2 closed five of the eight and raised three more: `B9`, `B10` and `B11`.
Review 3 is the acceptance gate for revision `9448227`: all eleven findings are
closed, and raised `F1` (command rejection had no channel) and `F2`
(`OpenSource` had no in-flight phase). The acceptance revision resolved both;
ADR-002 is now Accepted.

## Reading order for ADR-001

Read review 1 for the reasoning behind each finding, review 2 for the owner's
resolutions and the minimal type set, and review 3 for the acceptance gate. Each
round supersedes the previous one wherever they disagree, and each records the
positions it withdraws rather than dropping them.

Review 3 left one finding open, `F1`. The acceptance revision resolved it by
adding same-type relational comparisons, explicit zero values, and unary
negation for signed difference types. ADR-001 is now Accepted.
