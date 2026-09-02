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

## Reading order for ADR-002

Read review 1 for findings `B1` through `B8` and the reasoning behind each,
then review 2 for the owner's resolutions.

Review 2 closes five of the eight and leaves three items blocking: `B9`
(source-preview progress is not computable under ADR-001), `B10` (clip
selection is a presentation concern, not a transport command) and `B11`
(`HoldLastFrame` has no phase consistent with Stopped-implies-start). `B3` and
`B7` are partially resolved. ADR-002 remains Proposed.

## Reading order for ADR-001

Read review 1 for the reasoning behind each finding, review 2 for the owner's
resolutions and the minimal type set, and review 3 for the acceptance gate. Each
round supersedes the previous one wherever they disagree, and each records the
positions it withdraws rather than dropping them.

Review 3 left one finding open, `F1`. The acceptance revision resolved it by
adding same-type relational comparisons, explicit zero values, and unary
negation for signed difference types. ADR-001 is now Accepted.
