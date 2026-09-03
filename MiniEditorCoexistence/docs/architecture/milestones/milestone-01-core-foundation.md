# Milestone 1 — Core Foundation

**Goal:** create a buildable, testable boundary for the new playback engine
without changing what the editor displays or how it plays media.

This is intentionally a foundation milestone. It proves that the new engine can
be introduced without MFC, Qt, or a media backend leaking into its public
headers. Runtime playback migration begins in Milestone 3.

## Dependency graph

```text
M1-01  Framework-neutral PlaybackCore target + contract test
  |
  +-- M1-02  Rename the current UI-era PlaybackCommand
  |             to LegacyPlaybackCommand
  |
  +-- M1-03  Add the engine command values and the shared
                EditorIntent-to-command mapping contract
```

`M1-02` and `M1-03` may be worked on after `M1-01`; they touch different
responsibilities but are merged in that order to keep every commit buildable.

## M1-01 — Framework-neutral target

Create a static `MiniEditorPlaybackCore` C++17 library and a separate
`MiniEditorPlaybackCoreTests` executable. The initial core contains a small
header-only build-boundary contract rather than runtime playback behavior.

**Done when:**

- the target links only the C++ standard library;
- its public header includes no Qt, MFC, Win32, codec, or graphics header;
- its test target builds and runs through CTest with `MINI_EDITOR_USE_QT=ON`
  and `OFF`;
- the existing application behavior is unchanged.

## M1-02 — Retire the conflicting command name

Rename the existing four-member UI transport enum from `PlaybackCommand` to
`LegacyPlaybackCommand`. It remains the command spoken by the current
`IPlaybackBackend` path during coexistence.

**Done when:**

- all current callers preserve Toggle/Stop/Step behavior;
- no source outside the new core exposes the old ambiguous name;
- existing core and Qt widget tests pass.

## M1-03 — Establish the engine command boundary

Add the framework-neutral eight-alternative engine `PlaybackCommand` value
type from ADR-007 and one pure translation function for playback-related
`EditorIntent` values. The mapping must resolve Toggle from a supplied
published playback phase; lifecycle commands remain outside UI intent.

**Done when:**

- tests cover Toggle-to-Play and Toggle-to-Pause plus Stop, step, seek, and
  rate intent cases available in this milestone;
- invalid/non-playback intents cannot silently become playback commands;
- no UI framework type appears in the translator's API.

## Human decision gates

No decision is needed for M1-01 or M1-02: both directly apply accepted
ADR-007. Before M1-03 merges, a reviewer checks only that the proposed command
value shape still matches ADR-002 and ADR-007. A new product requirement,
additional transport action, or change to the approved command variants pauses
the automation and requires a human decision.

## Agent handoff rule

An implementation agent may take exactly one ready issue. It reports the
changed files, build/test commands, and any decision gate it encountered. The
next ready issue starts only after its predecessor has been reviewed and
merged, so `main` remains a known-good learning baseline.
