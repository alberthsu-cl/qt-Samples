# Milestone 5 — Rollout and migration

**Goal:** make the new playback engine the default for timeline preview —
after proving, with evidence rather than impression, that it behaves
equivalently to the legacy path — and then retire MFC timer transport
advancement.

**Status: planned. Implementation is gated on plan approval.**

This is the first milestone whose end state changes what the application does
by default. Milestones 1–4 were additive and inert: every new capability sat
behind a flag defaulting off. Here the flag flips, so the ordering below puts
all behavior-changing work *after* an automated comparison gate.

## What Milestone 4 left for this one

Three things Milestone 4 built are tested but not wired into any production
path, plus one migration step never started:

- `PreviewPresentationCoordinator` (M4-01) and `VideoWorkScheduler` (M4-03)
  exist with full fake-port coverage, but `TimelineEngineRouter` drives the
  Qt worker directly and bypasses both.
- `TimelinePlaybackResolver` still reads live `TimelineModel`/`MediaLibrary`
  state — ADR-003's migration step 3 ("adapt the resolver to consume only a
  snapshot") is unstarted. That is precisely why M4-06's routed preview
  resolves only the first V1 clip.
- Legacy playback mutators are still called from `PlaybackBackend.cpp`,
  `PlaybackClockController.cpp`, and `QtMediaPlaybackBackend.cpp`.

## Settled decisions

These were decided before planning rather than discovered mid-implementation:

- **A — Adopt the M4 machinery.** `PreviewPresentationCoordinator` and
  `VideoWorkScheduler` are wired into the production routed path in this
  milestone, not deferred again.
- **B — Dedicated presentation path inside the existing Qt preview panel.**
  The engine gets its own presentation surface within the panel. The legacy
  shared `QVideoSink` is *not* redirected, so the two paths never contend for
  one sink.
- **C — A1 audio gets its own worker-owned `QMediaPlayer`/`QAudioOutput`,**
  matching the legacy two-player topology. Audio-master clock timing stays
  deferred (ADR-004's milestone-1 `steady_clock` exception still applies).
- **D — Default-flip bar.** All automated comparison scenarios pass; fake-clock
  transport traces match **exactly (zero-frame tolerance)**; terminal states
  identical; no legacy timeline mutator called on the new path; plus human
  visual/audio validation. A compile-time legacy fallback is retained through
  the next E2E/regression milestone.
- **E — Source-asset preview stays on the legacy path for all of Milestone 5.**
- **F — Visual/audio validation is blocking. No pixel-perfect rendering
  comparison is required in this milestone.**

## Dependency graph

```text
M5-01  Snapshot-consuming timeline resolver
  |
  +-- M5-02  Adopt coordinator + scheduler; multi-clip routed playback   (A)
        |
        +-- M5-05  Dedicated engine presentation path in the Qt panel    (B)
        |     |
        +-- M5-06  A1 audio via a second worker-owned player             (C)
              |
M5-03  Session lifecycle across project reload                           |
  |                                                                      |
  +-- M5-04  PlaybackStatus -> read-only UI cache                        |
        |                                                                |
        +----------------+-----------------------------------------------+
                         |
                         +-- M5-07  Regression comparison harness  [AUTOMATED GATE]
                               |
                               +-- M5-08  Flip the default              (D, + manual validation)
                                     |
                                     +-- M5-09  Retire MFC timer advancement  (E)
```

M5-03 is independent of M5-01/M5-02 and may proceed in parallel.

## M5-01 — Snapshot-consuming timeline resolver

ADR-003 migration step 3, and the prerequisite for everything else: playback
resolution must read an immutable snapshot, never live editor containers.

**Scope**

- A resolver that maps a `TimelineFrame` to the active V1/A1 clip plus its
  resolved source time, reading only a `SequencePlaybackSnapshot`.
- The existing `TimelinePlaybackResolver` stays untouched and keeps serving
  the legacy path until M5-09.

**Non-goals**

- No production wiring yet (that is M5-02); no change to the legacy resolver.

**Done when**

- fake-port tests cover clip boundaries, gaps, still images, trimmed
  source-in, V1-only/A1-only/both, empty snapshots, and out-of-range frames;
- the new resolver has no dependency on `TimelineModel`, `MediaLibrary`, or
  `EditorSession` (ADR-003 criterion 4).

Architecture: ADR-003 (criteria 4, 13), ADR-007 (framework neutrality).

## M5-02 — Adopt coordinator and scheduler; multi-clip routed playback

Decision A. Replaces `TimelineEngineRouter`'s direct worker-drive with the
machinery already built and tested in Milestone 4, and lifts the
"first V1 clip only" limitation.

**Scope**

- Route frame requests through `PreviewPresentationCoordinator` (presentation
  identity, precedence policy) and `VideoWorkScheduler` (bounded latest-wins
  decode) instead of calling the worker directly.
- Use M5-01's resolver so the preview follows the playhead across clip
  boundaries, including gaps and still images.

**Non-goals**

- No preview-surface change (M5-05); no A1 audio (M5-06).

**Done when**

- playback crosses clip boundaries without a stall or a wrong-clip frame;
- scrubbing while paused is bounded latest-wins (at most one in-flight, one
  pending) and stale results are discarded by identity;
- gaps and still images resolve without a decoder error.

Architecture: ADR-003 (criteria 6, 7, 9, 10, 11), ADR-004 (scheduler loop).

## M5-03 — Session lifecycle across project reload

Fixes a latent defect from M4-06: the engine's `SequencePreview` identity is
fixed at construction, but `replaceProject()` mints a fresh `SequenceId`, so
after any project load the session's identity no longer matches the snapshots
being installed into it.

**Scope**

- Retarget or recreate the engine's session when `ProjectRuntime` produces a
  new `SequenceId` (project new/open/reload).
- Enforce ADR-003's rule that `InstallSnapshot` rejects a revision that is not
  strictly newer for the same `SequenceId`, while allowing any valid revision
  for a sequence not yet installed.

**Done when**

- loading a project twice in one run leaves the session's identity consistent
  with the installed snapshot;
- a duplicate or out-of-order install cannot roll playback content backward;
- deterministic tests cover reload, new-project, and out-of-order install.

Architecture: ADR-003 (sequence revision, snapshot installation), ADR-006.

## M5-04 — PlaybackStatus to read-only UI cache

The routed path currently leaves `EditorSession`'s timeline `PlaybackState`
stale, so the playhead and transport readouts do not track the new engine.
ADR-002 explicitly permits that state to survive **as a painting cache
populated from `PlaybackStatus`** — never written back as authority.

**Scope**

- Populate the timeline playback cache from published `PlaybackStatus` only,
  on the GUI thread, via the existing notification bridge.
- The cache is never read back into the engine, and no legacy mutator becomes
  an authority path.

**Done when**

- with routing on, the timeline playhead and transport readouts follow the
  engine;
- a test proves the cache is only ever written from a status publication;
- no legacy `EditorSession` playback mutator is called for timeline preview.

Architecture: ADR-002 (migration strategy; "read-only UI presentation cache").

## M5-05 — Dedicated engine presentation path in the Qt preview panel

Decision B. Replaces M4-06's standalone window with a real in-panel surface,
without touching the legacy shared sink.

**Scope**

- A presentation surface inside the existing Qt preview panel that the engine
  path owns, distinct from the legacy `QVideoSink` that
  `QtMediaPlaybackBackend` drives for source preview.
- Accepted frames reach it via the coordinator's acceptance rule; the renderer
  reports `FramePresented` without advancing transport.

**Non-goals**

- The legacy sink keeps serving source preview unchanged (Decision E). No
  sink redirection, no shared ownership.

**Done when**

- routed timeline playback renders in the app's own preview panel;
- source preview continues to render through the legacy path with no visible
  change;
- `FramePresented` is distinguishable from decode readiness and never
  advances transport (ADR-003 criterion 12).

Architecture: ADR-003 (presentation adapters), ADR-007 (Qt adapter).

## M5-06 — A1 audio via a second worker-owned player

Decision C. Mirrors the legacy two-player topology inside the new worker.

**Scope**

- A second `QMediaPlayer`/`QAudioOutput` pair owned by the worker thread for
  the A1 audio lane, resolved from the snapshot by M5-01's resolver.
- Audio and video are scheduled independently; the audio path never waits on
  video (ADR-004).

**Non-goals**

- Audio-master clock selection and drift correction stay deferred; the
  monotonic `steady_clock` remains the master (ADR-004's documented
  milestone-1 exception).

**Done when**

- a timeline with V1 video and A1 audio plays both concurrently;
- audio fades/mix state from the snapshot are applied;
- no audio-callback path blocks, allocates, or calls UI code (ADR-005).

Architecture: ADR-004 (audio policy, deferred master clock), ADR-005 (audio
callback boundary).

## M5-07 — Regression comparison harness — the automated gate

**This issue's scenario matrix is the automated gate for the default flip.**
Its purpose is to make Decision D's bar checkable rather than argued.

**Method**

Both paths are driven through the *same* `EditorIntent` script and their
observable traces are diffed:

- **Determinism.** Legacy advances one frame per `advanceOneFrame()` tick. The
  new path derives position from `IPlaybackClock`, so the harness injects a
  fake clock and steps it exactly one frame-time per legacy tick. Neither side
  depends on wall-clock timing.
- **Trace.** Per scenario: the ordered sequence of `(phase, timelineFrame)`
  samples, the terminal state, and an assertion that no legacy `EditorSession`
  timeline mutator fired on the new path.
- **Equivalence bar (Decision D).** Phase sequences identical; `timelineFrame`
  identical at every sample — **zero-frame tolerance**; terminal states
  identical.

**Scenario matrix**

| # | Scenario |
| --- | --- |
| 1 | Play from stopped |
| 2 | Pause, then resume |
| 3 | Seek while playing |
| 4 | Seek while paused |
| 5 | Seek while stopped |
| 6 | Step forward / backward at both boundaries |
| 7 | Clip-boundary crossing during playback |
| 8 | Still image on V1 |
| 9 | Gap on V1 |
| 10 | Trimmed source-in |
| 11 | End of timeline (returns to frame zero; next Play works) |
| 12 | Rate changes at 0.5x, 1.0x, 1.5x, 2.0x |
| 13 | Snapshot replacement mid-playback (edit while playing) |
| 14 | Undo / redo while paused |
| 15 | Project reload |
| 16 | Selecting a clip while paused leaves the playhead and phase unchanged |

**Also required**

- Existing `MiniEditorCoreTests` and `MiniEditorQtWidgetTests` pass
  **unmodified** with routing enabled (ADR-002 criterion 9).

**Non-goals**

- No pixel-perfect rendering comparison (Decision F). Visual fidelity is
  confirmed by human validation at M5-08, not by image diffing.

**Done when**

- every scenario above runs in CI-style automation and produces a readable
  pass/fail matrix;
- the matrix is green under the zero-tolerance bar.

Architecture: ADR-002 (criteria 9, 13), ADR-003 (criterion 6).

## M5-08 — Flip the default

Decision D. The behavior-changing commit, deliberately kept separate from
cleanup so it is trivially revertible.

**Scope**

- Timeline preview routes through the new engine by default.
- A compile-time legacy fallback is retained through the next E2E/regression
  milestone.

**Done when — all of the following**

- M5-07's scenario matrix is fully green (zero-frame tolerance);
- terminal states identical across all scenarios;
- no legacy timeline mutator is called on the new path;
- **human visual/audio validation has passed** (blocking, per Decision F);
- the legacy fallback still builds and runs when selected.

Architecture: ADR-002 (migration step 4), ADR-007 (feature flags).

## M5-09 — Retire MFC timer transport advancement

**Scope**

- Remove timeline transport advancement from the MFC timer; any remaining UI
  timer is a repaint heartbeat that samples published status only (ADR-002).
- Narrow the legacy `EditorSession` timeline mutators that no longer have a
  caller.

**Non-goals**

- Source-asset preview stays on the legacy path (Decision E), so
  `IPlaybackBackend`/`QtMediaPlaybackBackend` are **not** removed in this
  milestone — only timeline transport advancement is retired.
- The compile-time legacy fallback stays (Decision D).

**Done when**

- no MFC timer tick advances timeline transport;
- `QCoreApplication::processEvents()` is not used to make playback progress;
- the legacy path still works for source preview and for the retained
  fallback.

Architecture: ADR-002 (migration steps 5–6), ADR-005 (no UI-timer transport).

## Human decision gates

Decisions A–F are settled above. What remains genuinely open, and must pause
the work if reached:

- **A behavioral difference that is arguably a legacy bug.** If M5-07 finds
  the two paths differ and the *legacy* behavior looks wrong, matching it
  versus fixing it is a product decision, not an implementation choice.
- **A legacy behavior with no new-path equivalent** surfacing during M5-09's
  cleanup.
- **The visual/audio validation itself** at M5-08 — blocking, and not
  something automation can stand in for.

Anything else — a new transport command, a change to an accepted ADR
contract, or a change to this milestone's scope — also pauses the work.

## Agent handoff rule

An implementation agent may take exactly one dependency-ready issue, and only
after the plan is approved. It reports changed files, build/test commands, and
any decision gate it encountered. The next issue starts only after its
predecessor is reviewed and merged, so `main` stays a known-good baseline.
