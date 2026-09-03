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
                to LegacyPlaybackCommand
```

`M1-02` follows `M1-01` so every commit remains buildable while the new engine
command name is reserved.

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

## Deferred engine-command boundary

The eight-alternative engine `PlaybackCommand` and its shared
`EditorIntent`-to-command mapping begin in Milestone 3. `Seek` needs the
strong time and project/runtime identity values established by Milestone 2;
Toggle also needs an immutable published playback phase to resolve to `Play`
or `Pause`. A payloadless variant today would compile, but would not express
the accepted contract honestly.

## Human decision gates

No decision is needed for M1-01 or M1-02: both directly apply accepted
ADR-007. A new product requirement, additional transport action, or change to
the approved command variants pauses the automation and requires a human
decision.

## Agent handoff rule

An implementation agent may take exactly one ready issue. It reports the
changed files, build/test commands, and any decision gate it encountered. The
next ready issue starts only after its predecessor has been reviewed and
merged, so `main` remains a known-good learning baseline.
