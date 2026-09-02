# Timeline Playback Engine ADRs

Architectural Decision Records capture stable contracts that implementation
issues and agents may depend on.

An ADR moves through these states:

- **Proposed** — ready for review, but implementation must not depend on it.
- **Accepted** — the decision is a project contract.
- **Superseded** — a newer ADR replaces it; history remains readable.

## Decision index

| ADR | Decision | Status |
| --- | --- | --- |
| [ADR-001](0001-strong-media-time-domains.md) | Strong timeline, source, sequence, and master-clock time domains | Accepted |
| [ADR-002](0002-playback-session-is-the-state-authority.md) | PlaybackSession is the sole playback-state authority | Proposed |

Future decisions will cover immutable snapshots, master-clock policy, thread
ownership, sequence identity, and framework boundaries. Playback authority is
already proposed in ADR-002.
