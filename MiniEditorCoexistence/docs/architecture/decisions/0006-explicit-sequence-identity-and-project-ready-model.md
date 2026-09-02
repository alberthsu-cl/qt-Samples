# ADR-006: Explicit Sequence Identity and Project-Ready Model

Status: Proposed

Date: 2026-09-02

## Context

ADR-002 distinguishes source-asset preview from sequence preview. ADR-003
requires every playback snapshot to carry a `SequenceId` and monotonic
`SequenceRevision`. The editor still begins life as a project with no timeline
sequence, and a project reload must not accidentally reuse the identity of the
previous runtime sequence.

Playback must therefore receive an explicit sequence identity rather than
inferring one from keyboard focus, selected widgets, a file name, or the
presence of timeline items. The model also needs a clear answer for an empty
or partially loaded project.

## Decision

The project owns a sequence-ready value. A sequence is a first-class domain
object with an explicit runtime identity:

```cpp
struct TimelineSequence {
    SequenceId id;
    std::string name;
    FrameRate frameRate;
    TimelineModel timeline;
};

struct ProjectRuntime {
    ProjectId projectId;
    std::vector<TimelineSequence> sequences;
    std::optional<SequenceId> activeSequenceId;
    ProjectReadiness readiness;
    std::optional<ProjectError> error;
};
```

`ProjectError` is a framework-neutral value. `error` is engaged if and only if
`readiness == ProjectReadiness::Failed`.

`SequenceId` is unique for the lifetime of one loaded project runtime. Loading
or reloading a project creates a new runtime sequence identity, even when the
serialized project content is identical. A sequence revision is then assigned
to each accepted playback-affecting editor state as required by ADR-003.

The active sequence is selected explicitly by project/editor state. Playback
never infers its sequence from UI focus, a selected media-library item, a
timeline widget, or the first available sequence.

`ProjectRuntime` deliberately differs from the target document's `EditorProject`
file-shaped sketch. The existing `EditorProject` represents serialized project
data; this value represents one loaded runtime. `MediaLibrary` stays outside
this struct because playback snapshots contain only descriptors referenced by
the active sequence.

For milestone 1, loading the existing flat project format synthesizes exactly
one default `TimelineSequence` at 30/1. Saving continues to write the existing
flat `timelineItems`; sequence `FrameRate` is not persisted. Creating or
deleting sequences is a target-state capability of this model, not a
milestone-1 UI requirement. Persisting a sequence frame rate is a prerequisite
before exposing any other sequence rate.

## Project readiness

Readiness is explicit and separate from playback phase:

```cpp
enum class ProjectReadiness {
    Loading,
    Ready,
    Empty,
    Failed
};
```

- `Loading` means project data is still being assembled; no snapshot is
  published from partial state.
- `Ready` means an active sequence exists and contains at least one timeline
  clip from which playback can resolve content.
- `Empty` means the project loaded successfully but has no active sequence, or
  its active sequence has zero timeline clips. It is valid state, not an error.
- `Failed` means project loading or validation failed and carries a
  framework-neutral project error outside the sequence value.

An empty project produces an explicit empty sequence snapshot only when the
editor has created an active sequence. Otherwise playback receives no sequence
source and remains stopped; source-asset preview remains independently
available under ADR-002.

## Identity and lifecycle rules

1. A new project runtime creates new `ProjectId` and `SequenceId` values.
2. A project reload never reuses the previous runtime `SequenceId`, even if the
   file contents are unchanged.
3. Creating or deleting a sequence changes project structure and selects an
   active sequence explicitly; it does not rely on UI focus.
4. A playback-affecting edit within one sequence creates a strictly newer
   `SequenceRevision`; view-only changes do not.
5. Installing a snapshot with an unknown `SequenceId` is allowed only for the
   first snapshot of a newly introduced runtime sequence. For an already known
   sequence, revisions must be strictly newer as specified by ADR-003.
6. Closing a project clears the active sequence before the old runtime is
   destroyed and invalidates playback work through ADR-002/ADR-003.

A project reload preserves `PlaybackSessionId` when the existing engine session
continues to run, but it advances `PlaybackGeneration` and creates a new
project runtime, sequence identities, and revisions. Project-runtime recreation
also creates a new `PresentationSessionId` for the preview coordinator, as
defined by ADR-003. A new `PlaybackSessionId` is created only when the
`PlaybackSession` itself is recreated.

`SequenceId` and `SequenceRevision` are runtime identities, not persisted
project-format fields in the current migration. Persisted sequence names,
ordering, and future stable project keys may be mapped to new runtime IDs on
load.

## Snapshot and playback boundary

The project/editor thread builds a `SequencePlaybackSnapshot` only from one
completed, active `TimelineSequence`. The snapshot contains the sequence ID,
revision, frame rate, duration, referenced media descriptors, and normalized
video/audio clips required by ADR-003.

If no active sequence exists, the engine receives an explicit “no sequence
source” command or remains stopped according to ADR-002; it must not silently
play the first library asset. Selecting a library asset creates a
`SourceAssetPreview` request, not a `SequencePreview` request.

## Thread and framework boundaries

Project runtime and sequence values are framework-neutral. Qt and MFC adapters
may display readiness, sequence selection, and project errors, but they do not
own `SequenceId`, assign revisions, or choose playback source implicitly.
Project loading occurs at the editor boundary; the engine receives only a
completed value or an explicit error publication.

## Acceptance criteria

ADR-006 is implemented when:

1. Project reload creates a new runtime `ProjectId` and `SequenceId`, including
   reload of identical serialized content.
2. Playback requests explicitly identify `SourceAssetPreview` or
   `SequencePreview`; no request derives sequence identity from UI focus.
3. Loading, ready, empty, and failed project states are distinct and testable.
4. Partial project data cannot produce a playback snapshot.
5. Empty projects do not autoplay the first media-library asset.
6. Sequence creation, deletion, activation, and reload have deterministic
   identity and revision tests.
7. Snapshot construction uses only the explicitly active sequence and obeys
   ADR-003 revision rules.
8. Qt and MFC adapters display the same readiness and sequence publications
   without becoming project or playback authorities.
9. Core identity and readiness tests run without Qt Widgets, MFC windows, real
   codecs, or hardware.

## Consequences

The editor can safely support multiple sequences later, and project reloads
cannot let old asynchronous work appear current by identity reuse. Empty and
loading states become ordinary domain states rather than UI special cases.
The migration must add explicit project-runtime bookkeeping and sequence
selection, but the playback engine receives simpler, unambiguous inputs.

## Deferred decisions

Persisted stable sequence keys, multi-sequence editing UX, project-format
migration, and cross-sequence transitions are deferred. Framework-specific
project dialogs and notification widgets remain adapter concerns under ADR-007.
