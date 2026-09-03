# Playback-engine implementation milestones

This folder turns the accepted architecture decisions into small, reviewable
implementation steps. It is the practical bridge between:

- [`../decisions/`](../decisions/README.md) — **why** the engine has its
  contracts;
- [`../target-playback-architecture.md`](../target-playback-architecture.md)
  — **what** the finished architecture should look like;
- milestone documents here — **when and in what dependency order** code is
  introduced.

## Roadmap

| Milestone | Outcome | Status |
| --- | --- | --- |
| 1. Core foundation | A framework-neutral C++ playback-core target and the explicit `LegacyPlaybackCommand` name for the existing UI path. | **Complete** |
| 2. Project + snapshots | Strong time/identity values plus immutable project-runtime and sequence snapshots. | **Complete** |
| 3. Playback authority | `PlaybackSession`, injected fake clock, engine commands, and stale-result rules. | **Complete** |
| 4. Media integration | Decoder, audio, compositor, and MFC/Qt notification adapters behind a feature flag. | Planned |
| 5. Rollout + migration | Route timeline preview through the new core, compare behavior, make it default, and retire legacy timer advancement. | Planned |

## How to read and use a milestone

Each milestone document identifies its small issues, their dependency order,
acceptance checks, and any human decision gate.

1. Read the linked ADRs first; they are the accepted design constraints.
2. Take only a dependency-ready issue.
3. Keep the change focused and verify its stated acceptance checks.
4. Commit a known-good result before starting the next issue.
5. Pause for human review only when the work reaches a decision gate,
   contradicts an ADR, or reveals a product/behavior choice.

An agent can implement and test a ready issue. It should not silently invent a
new command, time domain, or authority rule; that requires an ADR update and a
human decision.

## Current milestone

[Milestone 1 — Core Foundation](milestone-01-core-foundation.md),
[Milestone 2 — Project and snapshots](milestone-02-project-and-snapshots.md),
and [Milestone 3 — Playback authority](milestone-03-playback-authority.md)
are complete. `PlaybackSession`, the injected clock/anchor, and the ADR-002
engine `PlaybackCommand` set now exist entirely behind fake ports. Milestone 4
adds the engine thread, decoder, audio device, and UI notification bridge
that this milestone's identity and state-machine values were built for;
nothing routes through the new session yet, so current application behavior
is unchanged.
