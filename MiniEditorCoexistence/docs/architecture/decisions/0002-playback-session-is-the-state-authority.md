# ADR-002: PlaybackSession Is the Playback-State Authority

Status: Proposed

Date: 2026-09-01

## Context

The current application stores two mutable playback states in `EditorSession`:
one for a selected source asset and one for the timeline. UI focus chooses which
state `playbackState()` exposes.

Authority then changes by context:

- real source preview takes position from `QMediaPlayer` callbacks;
- timeline preview advances through the MFC timer and
  `SimulatedPlaybackBackend`;
- still images and gaps also use the simulated timer;
- the V1 and A1 players maintain their own media clocks;
- UI selection changes which mutable state is considered active.

This hybrid was useful while migrating the UI incrementally, but it prevents a
clear answer to “what is the current playback position?” It also couples edit
selection to transport identity and makes asynchronous media callbacks capable
of writing into editor-owned state.

## Decision

One `PlaybackSession` is the sole mutable authority for the transport state of
one explicit `PlaybackSource`.

```cpp
using PlaybackSource = std::variant<
    SourceAssetPreview,
    SequencePreview>;

enum class PlaybackPhase {
    Stopped,
    Seeking,
    Prerolling,
    Playing,
    Paused,
    Failed
};

struct PlaybackStatus {
    PlaybackSessionId sessionId;
    PlaybackGeneration generation;
    PlaybackSource source;
    PlaybackPhase phase;
    TimelineFrame timelineFrame;
    int ratePercent = 100;
};
```

The session owns:

- active playback source;
- current phase;
- authoritative timeline/source position;
- playback rate;
- master-clock anchor;
- active snapshot revision;
- current generation;
- end and failure state.

All state-changing playback commands are serialized by the playback engine
thread:

```cpp
using PlaybackCommand = std::variant<
    OpenSource,
    InstallSnapshot,
    Play,
    Pause,
    Stop,
    Seek,
    SetRate,
    Shutdown>;
```

No UI object, editor controller, decoder, player callback, or timer may mutate
the session directly. They submit commands or observations to the engine.

The engine publishes immutable `PlaybackStatus` values. UI code may cache the
latest status for painting controls, but that cache is not authoritative and
cannot be written back as engine state.

## Separation from EditorSession

`EditorSession` remains authoritative for:

- editable project and sequence data;
- media-library state;
- clip selection and editing focus;
- undo/redo history;
- workspace-facing edit state.

`PlaybackSession` remains authoritative for:

- running/paused/stopped state;
- playback time and rate;
- source/sequence being previewed;
- seek and preroll generation;
- playback completion or failure.

An editor action may cause a playback command—for example, selecting a source
asset may submit `OpenSource`. This is an explicit request. Merely changing
keyboard focus cannot switch the engine's authoritative state implicitly.

## Player and decoder callbacks are observations

`QMediaPlayer`, future decoder workers, the audio device, and the compositor
report observations such as:

- source loaded;
- decoded frame ready;
- audio clock position;
- end of source;
- media failure.

The session decides whether an observation belongs to its current source and
generation and whether it causes a state transition. A media backend never
publishes a new authoritative UI position on its own.

In particular, a `QMediaPlayer::positionChanged` callback may help an adapter
report decoder progress during migration, but it does not replace the
session's master-clock position.

## UI timers are presentation heartbeats only

The MFC playback timer will no longer advance one timeline frame. A UI timer or
posted message may request repainting of the playhead and transport controls,
but it samples the latest `PlaybackStatus`.

If UI repainting is delayed, playback time continues according to the engine's
master clock. When the UI resumes, it displays the newest status rather than
replaying missed timer ticks.

## Session identity and generation

`PlaybackSessionId` identifies the lifetime of one playback session.
`PlaybackGeneration` identifies the current asynchronous epoch within that
session.

The generation advances when operations invalidate previously requested media,
including:

- seek;
- playback-source replacement;
- sequence-snapshot replacement;
- stop/shutdown when pending work must be discarded.

Session and generation identity travel with every asynchronous request, result,
and UI publication. ADR-003 will define snapshot and generation invalidation in
detail.

## State-transition ownership

Only `PlaybackSession` implements the playback state machine. Adapters translate
framework events into engine observations; they do not reproduce transition
logic.

Important first-milestone behavior remains explicit:

- Play from `Stopped` enters preroll before `Playing` when media is required.
- Pause freezes the authoritative position and retains the displayed frame.
- Resume continues from the paused position.
- Seek preserves the requested post-seek phase: paused seek returns to paused;
  playing seek prerolls and resumes playing.
- Stop invalidates pending work and returns timeline playback to its start.
- Timeline completion stops at the start so the next Play works immediately.
- Failure produces `Failed` with an error value rather than silently falling
  back to another authority.

Source-asset preview may retain its existing “hold the last frame” completion
policy until it migrates onto the new engine. That difference is selected by an
explicit source policy, never by UI focus.

## Why this decision

One authority turns playback behavior into a deterministic state machine. It
also makes thread ownership practical: commands enter one serialized queue,
and all state transitions happen on one engine thread.

The editor model and the playback engine can then evolve independently:

- editing produces snapshots and requests;
- playback consumes snapshots and publishes status;
- widgets present status and emit intent;
- decoders provide timestamped media observations.

## Consequences

Positive consequences:

- there is one answer for current phase, position, rate, source, and generation;
- focus and selection no longer choose playback authority;
- UI stalls cannot slow or restart media time;
- decoder callbacks cannot overwrite an intentional seek or source switch;
- state transitions become testable with fake clocks and fake decoders;
- multiple future sessions can coexist because identity is explicit rather
  than global.

Costs and limitations:

- playback commands become asynchronous from the UI's point of view;
- widgets must tolerate a short delay before receiving confirmed status;
- current `EditorSession` playback fields need a staged migration;
- adapters must distinguish commands from observations;
- shutdown must stop the session and join the engine thread explicitly.

## Migration strategy

During migration, the existing `PlaybackState` may remain as a read-only UI
presentation cache populated from `PlaybackStatus`. It must not remain an
independent authority.

The migration proceeds in this order:

1. Introduce `PlaybackSession` and test its state machine with fake ports.
2. Add a status adapter that updates existing transport/preview presentation.
3. Route timeline playback commands to the new session behind a compile-time
   feature flag.
4. Stop calling `EditorSession::advancePlaybackFrame()` for the new path.
5. Remove MFC timer advancement after regression parity is established.
6. Remove or narrow legacy playback mutation APIs after source preview also
   migrates.

The current `IPlaybackBackend` remains a temporary façade so visible UI code
does not change during this migration.

## Alternatives considered

### Keep EditorSession as the playback authority

Rejected because `EditorSession` is an editing/history boundary. Giving it
engine-thread timing responsibility would mix mutable project state with
real-time state and complicate thread safety.

### Make QMediaPlayer the authority

Rejected because a timeline contains still images, gaps, trimmed sources, and
independent V1/A1 media. No single high-level player represents the composed
timeline.

### Keep the MFC timer as the authority

Rejected because UI message delivery is not a media clock. Delayed timer
messages cause drift, and the timer cannot measure audio-device progress.

### Let every backend own its own playback state

Rejected because changing media kind or preview context would change transport
semantics and recreate the current split-authority problem behind interfaces.

## Acceptance criteria

ADR-002 is implemented when:

1. One `PlaybackSession` owns phase, position, rate, source, and generation.
2. Playback-session mutation occurs only on its serialized engine context.
3. UI controls submit commands and display immutable status publications.
4. `EditorSession` and UI focus do not select or advance authoritative playback
   state.
5. Player/decoder callbacks enter the engine as identified observations.
6. The MFC/Qt UI timer does not advance timeline time.
7. Pause, seek, resume, stop, completion, and failure transitions have pure
   state-machine tests.
8. A stale observation cannot change session state after source or generation
   replacement.
9. The existing UI operates through the temporary backend/status adapters
   without visible redesign.

## Learning focus

Ownership answers who stores and mutates a value. Authority answers which value
is considered correct when components disagree. ADR-002 deliberately gives
both responsibilities for transport state to `PlaybackSession`; every other
component either requests a change or reports an observation.
