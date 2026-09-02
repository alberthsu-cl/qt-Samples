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
| [ADR-002](0002-playback-session-is-the-state-authority.md) | PlaybackSession is the sole playback-state authority | Accepted |
| [ADR-003](0003-immutable-playback-snapshots-and-generation-gated-presentation.md) | Immutable playback snapshots and generation-gated presentation | Accepted |
| [ADR-004](0004-audio-monotonic-master-clock-policy.md) | Audio/monotonic master-clock policy | Accepted |
| [ADR-005](0005-engine-decoder-and-ui-thread-ownership.md) | Engine, decoder, audio callback, and UI thread ownership | Accepted |

Future decisions will cover sequence identity and framework boundaries. Thread
ownership, master-clock policy, immutable snapshots, and presentation
generation are accepted in ADR-003 through ADR-005.
