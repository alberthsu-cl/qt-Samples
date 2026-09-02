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

## Index

| Review | Subject | Round | Verdict |
| --- | --- | --- | --- |
| [ADR-001 review 1](adr-001-review-1-time-domains.md) | Strong Media Time Domains | 1 — original | Accept with revisions |
| [ADR-001 review 2](adr-001-review-2-resolutions.md) | Owner resolutions to review 1 | 2 — resolutions | 4 of 5 blockers closed |

## Reading order for ADR-001

Read review 1 for the reasoning behind each finding, then review 2 for what is
still open and for the minimal type set milestone 1 should implement. Review 2
supersedes review 1 wherever the two disagree, and records one correction to it.
