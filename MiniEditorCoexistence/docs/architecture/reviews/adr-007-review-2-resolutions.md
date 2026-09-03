# ADR-007 Review 2 — Resolution Verification

Verdict: **Accepted with revisions**

Round: 2 (resolution review)

Revision reviewed: `699338b` *Revise ADR-007 after framework review*
(diff confined to ADR-007).

Reviewed against
[ADR-007](../decisions/0007-framework-neutral-core-and-ui-adapters.md) as it
stands after `699338b`, [review 1](adr-007-review-1-framework-boundary.md),
ADR-001 through ADR-006 (all six Accepted, closed),
[`../target-playback-architecture.md`](../target-playback-architecture.md),
and the current build (`CMakeLists.txt`, `src/EditorCommandController.h`,
`src/ProjectState.h`).

## Verdict

N1 through N4 are all resolved, three of them with wording that matches the
recommended fix almost verbatim. B1 is resolved by a genuinely better
mechanism than either option review 1 offered: rather than extracting
adapter-local translation functions or downgrading the criterion to
inspection, the revision routes both adapters through the shared,
already-existing `EditorIntent` type before either produces a
`PlaybackCommand` — which sidesteps the build-exclusivity problem entirely
instead of working around it, and does so by formalizing a mechanism the
codebase already runs today rather than inventing a new one.

Verifying that mechanism's feasibility, as this round was specifically asked
to do, surfaced one new blocking issue: `PlaybackCommand` already exists in
this codebase as a four-member `enum class`
(`TogglePlayPause, Stop, StepBackward, StepForward`), actively used by
`EditorSession` and the temporary `IPlaybackBackend` façade — a different
type from ADR-002's eight-member `std::variant` of the same name. The exact
code path ADR-007's fix relies on — extending `EditorCommandController` to
map `EditorIntent` to ADR-002's `PlaybackCommand` — is the first place in the
series where both types would need to be visible in the same translation
unit, which will not compile as things stand. This is the same class of
defect as ADR-003's `MediaKind` collision, caught here because the task
asked this round to verify feasibility against the actual build rather than
against the document's prose alone.

## Resolution status

| ID | Resolution applied | Verified against text |
| --- | --- | --- |
| B1 | Shared `EditorIntent` intermediate replaces direct per-adapter command construction | See detailed feasibility analysis below. **Resolved as a mechanism; surfaces B2.** |
| N1 | Qt adapter's two delivery mechanisms scoped by deployment | "In the current MFC-hosted coexistence shell, embedded Qt widgets are updated by the MFC notification-queue drain. In a future Qt-native shell, Qt-facing results use queued signals to the Qt GUI thread." **Resolved.** |
| N2 | Migration sequence cross-referenced against per-ADR sequences | "This migration sequence is the adapter-level view that integrates, rather than replaces, the finer migration strategies in ADR-002 through ADR-005." **Resolved.** |
| N3 | Criterion 8 names the existing test suites | "`MiniEditorCoreTests` and the Qt widget test suite preserve existing timeline playback, pause, seek, end-of-timeline, and stale-result behavior across MFC and Qt panel replacement configurations." Matches the recommendation and adds the cross-configuration clause unprompted. **Resolved.** |
| N4 | "queue or event-sink port" tightened to name one mechanism | "The core exposes a thread-safe UI notification queue through the `IPlaybackEventSink` port." **Resolved.** |

## Feasibility of the shared `EditorIntent` → `PlaybackCommand` strategy

**The mechanism itself is sound, well-grounded, and preserves the
one-authority rule.** This was checked against the actual codebase, not
assumed from the ADR's prose.

`EditorIntent` is not a new type this ADR invents — it already exists in
`src/EditorCommandController.h`, with a comment describing exactly the
property ADR-007 now relies on: "Intent-level commands shared by native MFC
menus/accelerators and Qt controls. These names deliberately do not contain
Win32 command IDs or Qt action types, so either UI framework can invoke the
same editor behavior." `EditorCommandController.h`/`.cpp` compile in the
common source list for both `CMakeLists.txt` configurations and in
`MiniEditorCoreTests`, which links no Qt and no MFC. This is confirmed, not
inferred: `grep` against `CMakeLists.txt` shows
`EditorCommandController.h`/`.cpp` listed once in the main executable's
unconditional sources and again in `MiniEditorCoreTests`'s source list,
outside either `if(MINI_EDITOR_USE_QT)` branch.

That means a test exercising "does `EditorIntent::X` map to `PlaybackCommand`
`Y`" can live entirely inside the zero-dependency core test target — the
same target this series has already relied on for ADR-001 through ADR-006's
analogous criteria — without requiring the MFC adapter and the Qt adapter to
ever be compiled together. Each adapter's own claim ("this Qt button click
produces `EditorIntent::TogglePlayback`" / "this MFC menu item produces
`EditorIntent::TogglePlayback`") is separately testable inside its own build
configuration; the shared mapping from `EditorIntent` to `PlaybackCommand` is
tested once, framework-free. Together these two independently-testable
claims transitively establish "the same user intent produces the same
command" without ever needing both full adapters in one binary — exactly
what the reworded criterion 3 now asks for, and exactly why this
sidesteps B1 rather than merely working around it.

**One-authority rule: preserved.** The `EditorIntent` → `PlaybackCommand`
step is a pure, stateless mapping — it decides which command to submit, not
what the engine's state is. It submits through the same single engine
command queue ADR-002 and ADR-005 already established; nothing about
introducing this intermediate value creates a second place phase or position
could be tracked. Where the mapping needs live information — `TogglePlayback`
resolving to `Play` or `Pause` depends on the last known phase — that is
exactly the kind of read-only status caching ADR-002 already permits UI code
to do ("UI code may cache the latest status for painting controls"), and
ADR-002's idempotent `Play`/`Pause` handling from either phase already covers
a stale guess resulting from a race.

## B2 (new) — `PlaybackCommand` name collision with an existing, differently-shaped type

*Whole document; the collision becomes concrete wherever `EditorIntent` is
mapped to ADR-002's `PlaybackCommand`.*

`src/ProjectState.h` already declares:

```cpp
enum class PlaybackCommand {
    TogglePlayPause,
    Stop,
    StepBackward,
    StepForward
};
```

This is actively used today — `EditorSession::handlePlaybackCommand()`,
`EditorCommandController.cpp`'s calls into `playbackBackend_.executeCommand()`,
and the temporary `SimulatedPlaybackBackend`/`IPlaybackBackend` façade all
depend on this exact four-member enum. ADR-002's `PlaybackCommand` is a
different type entirely — an eight-member `std::variant`
(`OpenSource, InstallSnapshot, Play, Pause, Stop, Seek, SetRate, Shutdown`) —
sharing only the name, in a codebase with no namespacing to separate them.

This has been true since ADR-002 was accepted, but ADR-007's fix is the
first place in the series where the collision becomes unavoidable: mapping
`EditorIntent` to ADR-002's `PlaybackCommand` inside (or alongside)
`EditorCommandController` is precisely the code that would need both
declarations visible in one translation unit. Two same-named,
differently-shaped types in one program will not compile — the same failure
mode ADR-003's original `MediaKind` redeclaration produced, caught here
before it reached code rather than after.

This does not reopen ADR-002, which never declared a C++ symbol name for
its `PlaybackCommand` — the collision is with code the current migration
already carries, and the resolution belongs to whichever document actually
connects the two, which is this one.

**Minimal fix.** State that the existing four-member `PlaybackCommand` enum
is retired or renamed as part of this migration. ADR-002 already frames its
owner as temporary — "The current `IPlaybackBackend` remains a temporary
façade so visible UI code does not change during this migration. Its
implementation will delegate commands to the new engine rather than growing
more reconciliation logic" — so the legacy enum's lifetime is already
bounded by an accepted decision; this ADR only needs to say so explicitly,
or give the legacy type a distinguishing name (for example
`LegacyPlaybackCommand`) for the window during which both must exist. No new
design decision is required either way.

## Non-blocking improvement carried over from the same investigation

**N5 — `EditorIntent` does not yet cover the full `PlaybackCommand` surface.**
The existing enum has `TogglePlayback` and `StopPlayback`/`StepBackward`/
`StepForward`, with no member corresponding to `Seek`, `SetRate`,
`OpenSource`, `InstallSnapshot`, or `Shutdown`, and `TogglePlayback` maps to
two different `PlaybackCommand` alternatives (`Play` or `Pause`) depending on
last-known phase rather than one-to-one. None of this blocks acceptance —
extending an enum and writing a phase-aware mapping case are implementation
tasks, not design decisions — but the ADR's phrasing ("shared translation
maps that intent to the appropriate `PlaybackCommand`") reads as already
complete. One sentence acknowledging that `EditorIntent` requires new members
for the remaining commands would set that expectation correctly.

## New checks for this round

**Contradictions.** None beyond B2, which is a build-graph defect rather than
a textual disagreement between documents. **Missing ownership rules.** None
found. **Unclear behavior.** None found beyond N5's minor completeness gap.
**Untestable acceptance criteria.** Criterion 3 is now testable in mechanism;
its testability in practice is contingent on B2's fix landing first, since
the mapping code it describes cannot be written otherwise. **Scope
expansion.** None — the `EditorIntent` mechanism formalizes existing code
rather than adding new milestone-1 requirements.

## Is ADR-007 implementation-ready?

**Not quite — one item remains, and it is a one-paragraph fix.** B2 must be
addressed before the mapping code this ADR describes can be written at all,
since it is a compilation blocker, not a design gap. N5 is optional
completeness and does not gate acceptance.

## Status change

**ADR-007 may change from Proposed to Accepted once B2 is resolved.** N5 may
be applied in the same pass or left for later.

## Reviewer's note

This round is the first in the series where "verify feasibility against the
actual build" was the explicit instruction rather than an optional check,
and it found something a purely textual comparison against the other ADRs
would not have: a symbol collision that has been latent in the codebase
since ADR-002 was accepted, invisible until a document actually proposed
connecting the two names. The fix costs one sentence. Finding it cost
reading three source files instead of six markdown files — worth doing
again whenever a resolution's feasibility, not just its wording, is the
question being asked.
