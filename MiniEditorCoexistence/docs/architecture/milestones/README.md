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
| 4. Media integration | Decoder, audio, compositor, and MFC/Qt notification adapters behind a feature flag. | **Complete** |
| 5. Rollout + migration | Route timeline preview through the new core, compare behavior, make it default, and retire legacy timer advancement. | In progress — [plan](milestone-05-rollout-and-migration.md); default flipped, timer retirement outstanding |

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
[Milestone 3 — Playback authority](milestone-03-playback-authority.md), and
[Milestone 4 — Media integration](milestone-04-media-integration.md) are all
complete. Milestone 4's eight issues (M4-01 through M4-08) introduced this
application's first real background thread, its first `PostMessage`/
`ON_MESSAGE` notification bridge, its first real Qt Multimedia adapter, and
feature-flagged routing of timeline preview through the new engine — so it
called out its higher-risk, real-integration issues explicitly throughout
rather than treating them as equivalent to the fake-port core work. Both
manual validations (real playback, and a real corrupt-file failure) passed,
and the failure path is now covered automatically by an end-to-end Qt test.

`MINI_EDITOR_ENABLE_ENGINE_SMOKE_TEST` still defaults off; it changes nothing
about real playback either way.

[Milestone 5 — Rollout and migration](milestone-05-rollout-and-migration.md)
is **in progress**. It is the first milestone whose end state changes default
behavior, so it put every behavior-changing step *after* an automated
comparison gate. That gate is now green: M5-07's sixteen scenarios match at
exact phase and frame equality with no legacy timeline mutator called on the
new path, and human visual/audio validation passed — so M5-08 has flipped
`MINI_EDITOR_ENABLE_ENGINE_ROUTING` to default `ON`. Source-asset preview
stays on the legacy path throughout, and the compile-time legacy fallback is
retained past the flip. Retiring MFC timer transport advancement (M5-09) is
still to come.
