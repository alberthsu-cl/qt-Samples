# Target Timeline Playback Architecture

Status: proposed design for review. No implementation issue should depend on
this document until its decisions are accepted and recorded as ADRs.

This design replaces the current split playback authority with one explicit
timeline playback engine. It preserves the existing UI and the useful pure C++
timeline-resolution work while making time, ownership, thread affinity, and
stale asynchronous work explicit.

## Communication goal

After reading this document, a contributor should be able to answer:

- which component owns playback state;
- how a timeline position maps to source media;
- what data may cross a thread boundary;
- how seek invalidates obsolete work;
- which clock controls presentation;
- how one engine instance selects one of several project sequences;
- where decoding, composition, audio output, and UI presentation belong.

## Scope of the first engine milestone

The first milestone supports:

- one active project sequence;
- one V1 video or still-image track;
- one independent A1 audio track;
- source in/out trimming;
- gaps;
- clip opacity and fades;
- the existing DSP settings;
- seek, play, pause, resume, stop, and rate change;
- real video and audio sources;
- stable stale-request rejection;
- 0.5x, 1.0x, 1.5x, and 2.0x preview rates.

The data model and engine contracts must permit multiple sequences and vectors
of resolved layers, but the first milestone does not add corresponding UI.

## Explicit non-goals

The first engine milestone does not implement:

- new visible editor UI;
- multiple visible video or audio tracks;
- nested sequences;
- transitions;
- GPU composition;
- offline export;
- background proxy generation;
- live capture.

These features must remain possible without changing the engine's ownership or
time contracts.

## Target component flow

```text
EditorProject
    -> TimelineSequence
    -> immutable SequencePlaybackSnapshot
    -> TimelineResolver
       -> resolved video layer(s)
       -> resolved audio layer(s)
    -> PlaybackSession + PlaybackScheduler
       -> VideoDecodeService
       -> AudioDecodeService
       -> bounded timestamped queues
    -> Compositor
    -> PreviewFramePublisher       AudioOutput
       -> UI-thread renderer       -> audio device
```

The UI sends commands into this flow. It never becomes part of the flow's time
or media authority.

## Decision 1: sequence identity is explicit

The project becomes sequence-ready:

```cpp
struct TimelineSequence {
    SequenceId id;
    std::string name;
    FrameRate frameRate;
    TimelineModel timeline;
};

struct EditorProject {
    MediaLibrary mediaLibrary;
    std::vector<TimelineSequence> sequences;
};
```

The editor may keep an `activeSequenceId` as workspace/session state. Playback
does not infer its sequence from keyboard focus or selected widgets.

A playback request explicitly names its source:

```cpp
using PlaybackSource = std::variant<
    SourceAssetPreview,
    SequencePreview>;
```

`SequencePreview` contains a `SequenceId`; `SourceAssetPreview` contains a
`MediaAssetId`. Source preview and timeline preview may use the same playback
engine without pretending they are the same kind of source.

### Consequence

Switching to another timeline later creates or retargets a playback session
with another sequence snapshot. Decoder, scheduler, compositor, and renderer
contracts do not change.

## Decision 2: time domains are strongly typed

The engine does not use one unqualified `int frame` for every time domain.

```cpp
struct TimelineFrame {
    std::int64_t value = 0;
};

struct SourceTimestamp {
    std::chrono::microseconds value{};
};

struct PresentationTime {
    std::chrono::microseconds value{};
};

struct FrameRate {
    std::int32_t numerator = 30;
    std::int32_t denominator = 1;
};
```

The roles are different:

- `TimelineFrame` expresses an edit position in one sequence's frame rate.
- `SourceTimestamp` expresses a decoder position in source-media time.
- `PresentationTime` expresses elapsed playback-clock time.

Conversions require a named function and a `FrameRate`. They are never
implicit. Frame-to-time conversion is calculated from the absolute value using
rational arithmetic; the engine does not accumulate rounded frame durations.

### Consequence

The compiler helps prevent sending a timeline position directly to a decoder.
The design can later support 24, 25, 29.97, 30, and other sequence rates without
changing playback ownership.

## Decision 3: playback session is the sole state authority

`PlaybackSession` owns the authoritative transport state for one
`PlaybackSource`:

```cpp
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

`EditorSession` remains the authority for editable project state, selection,
and history. It no longer owns the authoritative running playback clock.

The UI receives immutable `PlaybackStatus` publications. It may cache them for
painting, but it cannot advance or rewrite engine time.

### Consequence

Selection and keyboard focus may request a source change, but they do not
silently select which of two mutable playback states is authoritative.

## Decision 4: playback consumes immutable snapshots

The engine never reads live mutable `EditorSession`, `TimelineModel`, or
`MediaLibrary` data from a worker or scheduler.

The editor builds a self-contained snapshot:

```cpp
struct SequencePlaybackSnapshot {
    SequenceId sequenceId;
    SequenceRevision revision;
    FrameRate frameRate;
    TimelineFrame duration;
    std::vector<PlaybackMediaDescriptor> media;
    std::vector<PlaybackClip> videoClips;
    std::vector<PlaybackClip> audioClips;
    TimelineAudioMixState audioMix;
};
```

Snapshot members are value types or shared immutable media descriptors. No
widget, QObject, MFC window, callback, or reference to an editor container is
stored in the snapshot.

When an edit affects playback:

1. the UI/editor thread completes the edit command;
2. it builds a new snapshot with a new sequence revision;
3. it submits the snapshot to the playback session;
4. the playback session advances its generation;
5. old queued decoder/compositor results become stale.

For the first milestone, applying a new snapshot is a scheduler command-boundary
operation. The session preserves the requested timeline position, clamps it to
the new duration, invalidates old work, and enters seek/preroll before publishing
another frame.

### Consequence

Undo, trim, split, fade, and DSP changes cannot race with playback reading a
partially modified timeline.

## Decision 5: every asynchronous operation carries identity

Every engine request and result carries:

- `PlaybackSessionId`;
- `PlaybackGeneration`;
- `SequenceId` or source-preview identity;
- media/clip identity where relevant;
- requested source or presentation timestamp.

Example:

```cpp
struct VideoDecodeRequest {
    PlaybackSessionId sessionId;
    PlaybackGeneration generation;
    MediaAssetId mediaAssetId;
    SourceTimestamp sourceTime;
    PresentationTime deadline;
};

struct DecodedVideoFrame {
    PlaybackSessionId sessionId;
    PlaybackGeneration generation;
    MediaAssetId mediaAssetId;
    SourceTimestamp sourceTime;
    VideoFrameBuffer buffer;
};
```

A consumer accepts a result only when both session and generation match its
current state. Cancellation is primarily generation-based: workers may finish
old work, but consumers discard it without publishing it.

Cooperative cancellation may reduce wasted decode time, but correctness must
not depend on a decoder stopping immediately.

### Consequence

The current seek request-ID idea becomes a general engine-wide contract rather
than a video-sink-only special case.

## Decision 6: the scheduler uses a master clock

Playback position is calculated from time; it is not advanced by adding one
frame each time a UI timer fires.

When audible audio is active, the audio-output clock is the master. When no
audio is active, the scheduler uses `std::chrono::steady_clock`.

Conceptually:

```text
timeline position = anchor position
                  + elapsed master-clock time * playback rate
```

The scheduler converts that position to the sequence's `TimelineFrame`, asks
the resolver what should be active, and chooses frames for presentation.

The scheduler policy includes:

- retain a small bounded queue of timestamped decoded frames;
- wait when the next video frame is early;
- present the newest frame not later than the target time;
- drop superseded late video frames;
- never block the audio callback waiting for video;
- pause by freezing the timeline anchor and clock relationship;
- seek by flushing queues and advancing generation;
- stop by invalidating work and returning to the defined start position.

The UI may use a low-cost display heartbeat to repaint the playhead, but that
heartbeat samples `PlaybackStatus`; it does not advance playback.

### Consequence

Temporary UI stalls do not redefine media time, and V1/A1 synchronization has
one measurable authority.

## Decision 7: thread ownership is explicit

The target application-level ownership is:

| Thread/context | Owns | Must not do |
| --- | --- | --- |
| MFC/Qt UI thread | `EditorSession`, project editing, widgets, preview presentation adapter | Decode media, wait for workers, mutate playback internals |
| Playback engine thread | `PlaybackSession`, scheduler, snapshot installation, queue policy | Access widgets or mutable editor containers |
| Video decode worker(s) | Video decoder contexts and their request queues | Access UI or choose timeline policy |
| Audio decode worker(s) | Audio decoder contexts and PCM production | Access UI or choose edit policy |
| Audio device callback | Consume ready PCM and report device clock | Allocate heavily, wait for video, call UI |

An implementation may initially use one video and one audio worker. The
interfaces do not require one thread per media asset.

Qt `QObject` instances are created, called, and destroyed on their owning
thread. A worker that requires a Qt event loop owns it for the worker's full
lifetime. Shutdown is an explicit command followed by thread join; no engine
QObject uses a UI parent.

### UI notification bridge

The engine publishes immutable status/frame notifications into a thread-safe
UI queue. The MFC host posts a Windows message to `MainFrame`, which drains the
queue on the UI thread. Qt widgets are updated from that UI-thread drain.

This avoids depending on the playback timer to call
`QCoreApplication::processEvents()` for engine correctness. Qt event-loop
integration can be improved separately without changing the engine protocol.

## Decision 8: resolution returns collections, even with one track

The first milestone renders one V1 and one A1 track, but the resolver result is
collection-oriented:

```cpp
struct ResolvedTimelineSample {
    TimelineFrame timelineFrame;
    std::vector<ResolvedVideoLayer> videoLayers;
    std::vector<ResolvedAudioLayer> audioLayers;
};
```

For now, each vector contains zero or one layer. Layer ordering and track IDs
are explicit. Supporting additional tracks later changes model rules and
composition work, not the resolver's return shape.

### Consequence

The architecture scales to multiple tracks without prematurely implementing
their UI or overlap policy.

## Decision 9: composition is separate from decoding

Decoders produce source frames and PCM. They do not apply timeline opacity,
position, fades, or effects.

The compositor receives:

- the resolved layer description;
- decoded source frame(s);
- requested output size and preview quality;
- the session/generation identity.

It produces one timestamped `CompositedVideoFrame` for presentation. The first
implementation may use CPU image processing. A later D3D11/QRhi compositor can
replace that implementation without changing snapshot, resolver, session, or
scheduler contracts.

Audio fade/mix policy likewise belongs after audio decode and before device
output.

### Consequence

The same edit decisions can later drive real-time GPU preview or offline export
without asking the decoder to understand timeline semantics.

## Decision 10: engine interfaces remain framework-neutral

The core depends on standard C++ value types and abstract ports:

```cpp
class IPlaybackClock;
class IVideoDecodeService;
class IAudioDecodeService;
class IVideoCompositor;
class IPlaybackEventSink;
```

Qt and MFC adapters live at the application boundary. Qt-specific frame handles
may exist inside a concrete decoder/compositor implementation, but they do not
leak into the timeline model, resolver policy, or session state machine.

### Consequence

Core state, timing, resolver, and stale-generation behavior can be tested
without launching MFC, Qt Widgets, a codec, or an audio device.

## State-machine contract

```text
Stopped
  play  -> Prerolling -> Playing
  seek  -> Seeking    -> Paused

Playing
  pause -> Paused
  seek  -> Seeking -> Prerolling -> Playing
  stop  -> Stopped at start
  end   -> Stopped at start

Paused
  play  -> Prerolling -> Playing
  seek  -> Seeking -> Paused
  stop  -> Stopped at start

Any active phase
  decode/output failure -> Failed
  new snapshot/source   -> generation++ -> Seeking/Prerolling
```

`Prerolling` means the session is waiting for enough media to start without an
immediate underrun. A still image or gap may complete preroll without a video
decoder.

The first milestone preserves the agreed timeline-end behavior: reaching the
end stops and returns to the start so the next Play command works immediately.

## Command and event boundary

Commands entering the engine are immutable values:

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

Events leaving the engine are immutable values:

```cpp
using PlaybackEvent = std::variant<
    StatusChanged,
    VideoFrameReady,
    PlaybackEnded,
    MediaFailed>;
```

Both boundaries carry session/generation identity. Commands are serialized by
the engine thread, which eliminates concurrent mutation of `PlaybackSession`.

## Multiple sequences and future nesting

`EditorProject` owns several `TimelineSequence` values. One UI workspace may
later open several sequence views, but each playback session explicitly names
one source.

Multiple independent preview sessions are possible because session identity is
not global. The first milestone creates only one session to avoid decoder and
audio-device contention.

A future nested sequence can be represented as another media-source variant.
The resolver would recurse through an immutable referenced sequence snapshot
with cycle detection and a maximum nesting policy. No nesting behavior is
implemented now.

## Migration path

The target engine is introduced behind the existing application boundary in
small steps:

1. Add strong IDs/time types and sequence-ready value models without changing
   runtime behavior.
2. Add immutable playback snapshots and adapt the existing resolver to consume
   them.
3. Add a deterministic `PlaybackSession` state machine with fake clock and fake
   decode ports.
4. Add the scheduler/master-clock policy and bounded queues under tests.
5. Add Qt media decode adapters and the MFC-to-engine notification bridge.
6. Route timeline preview through the new engine behind a compile-time flag.
7. Compare existing and new behavior using the same regression scenarios.
8. Make the new engine the default and remove timeline advancement from the MFC
   timer.
9. Retain source-preview behavior until it can move onto the same session
   protocol without destabilizing timeline playback.

The current `IPlaybackBackend` can serve as the temporary integration seam.
Its implementation will delegate commands to the new engine rather than
growing more reconciliation logic.

## Testing strategy

### Pure unit tests

- rational frame/time conversion;
- timeline/source domain separation;
- snapshot construction and revision behavior;
- resolver mapping across trim, still, gap, fade, and clip boundaries;
- state-machine transitions;
- generation rejection;
- scheduler selection/drop/wait policy with a fake clock;
- end-of-sequence and rate-change behavior.

### Thread-contract tests

- commands are serialized on the engine thread;
- worker results never mutate UI-owned state;
- shutdown cancels work and joins workers;
- late results after seek/source replacement are ignored;
- bounded queues do not grow without limit.

### Integration tests

- one real V1 video plus one real A1 audio;
- seek into a trimmed non-zero source position;
- pause retains the exact presented frame;
- resume continues from the paused timeline position;
- clip boundary switches media without moving backward;
- UI stalls do not cause the timeline clock to restart;
- timeline end returns to frame zero;
- existing transport and timeline UI remain visually unchanged.

## Architectural invariants

These rules are intended to become acceptance criteria and tests:

1. Exactly one `PlaybackSession` owns authoritative playback state.
2. The UI never advances playback time.
3. Workers never read mutable editor state or access widgets.
4. Playback resolves only immutable snapshots.
5. Every asynchronous request/result carries session and generation identity.
6. Seek/source/snapshot changes invalidate older generations.
7. Audio is the master clock when audible audio is active; otherwise a monotonic
   clock is used.
8. Video timing follows the master clock; audio never waits for video.
9. Timeline, source, and presentation times require explicit conversion.
10. Queue sizes are bounded and shutdown joins every application-owned worker.
11. A playback session identifies its source explicitly, never through UI
    focus.
12. Core engine tests do not require MFC, Qt Widgets, real codecs, or hardware.

## Proposed ADR set

Once this target is accepted, record its independently important decisions:

- ADR-001: Strong timeline, source, and presentation time domains.
- ADR-002: PlaybackSession as the sole playback-state authority.
- ADR-003: Immutable sequence snapshots and generation invalidation.
- ADR-004: Audio/monotonic master-clock policy.
- ADR-005: Engine, decoder, audio callback, and UI thread ownership.
- ADR-006: Explicit sequence identity and sequence-ready project model.
- ADR-007: Framework-neutral core with Qt/MFC boundary adapters.

## Review checklist

Before accepting this proposal, verify:

- Is any mutable state owned by more than one thread?
- Can any worker reach `EditorSession`, a widget, or a mutable timeline?
- Can an old decode result be mistaken for a new seek result?
- Does any UI timer still advance engine time?
- Can source time be passed where timeline time is expected?
- Does playback identity depend on focus or selection?
- Can one project hold several sequences without changing decoder interfaces?
- Can the state machine and scheduler run with fake clocks and fake decoders?
- Does the first milestone remain limited to one V1 and one A1 track?
- Can the current UI consume the engine without visible redesign?
