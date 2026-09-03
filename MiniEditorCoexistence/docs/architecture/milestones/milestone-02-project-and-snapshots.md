# Milestone 2 — Project and snapshots

**Goal:** introduce framework-neutral time, runtime identity, and immutable
sequence-snapshot values without moving playback execution to a new thread.

## Completed issues

1. **M2-01 — Strong media time domains**
   - Eight distinct value types prevent timeline, source, sequence, and clock
     time from being mixed accidentally.
   - Named conversions cover rational frame rates, source mapping, and playback
     rate conversion.
2. **M2-02 — Project runtime and sequence identity**
   - `ProjectRuntime` creates fresh `ProjectId` and `SequenceId` values for
     every new/reloaded legacy project.
   - The existing flat project format synthesizes one active 30/1 sequence;
     its readiness is explicitly Loading, Ready, Empty, or Failed.
3. **M2-03 — Immutable sequence playback snapshots**
   - The snapshot builder copies one completed editor state into immutable
     media descriptors and normalized V1/A1 clip vectors.
   - Missing media is explicit; invalid references reject the entire candidate.
   - Playback-affecting timeline edits advance `SequenceRevision` without
     changing `SequenceId`.

## What remains deliberately deferred

No `PlaybackSession`, engine thread, decoder, UI notification queue, or
feature-flagged runtime path exists yet. Those belong to Milestone 3 and later.
The completed snapshot is a safe input for that future authority; it does not
change the editor's current legacy playback behavior.

## Verification

The framework-neutral core tests and the existing editor/Qt-widget regression
tests pass in both Qt-enabled and MFC-only Release configurations.

*Architecture: ADR-001, ADR-003, ADR-006, ADR-007.*
