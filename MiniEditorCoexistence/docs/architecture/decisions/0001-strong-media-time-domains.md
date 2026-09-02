# ADR-001: Strong Media Time Domains

Status: Accepted

Date: 2026-09-01

Accepted: 2026-09-02 after independent review rounds 1 through 3

## Context

The current editor represents timeline positions, clip-local offsets, source
positions, and playback position as plain `int` values. Their meaning is carried
by names and comments rather than by the C++ type system.

That was sufficient for a fixed 30 fps learning UI, but the new engine must not
confuse these distinct concepts:

1. a position or length on a sequence frame grid;
2. a timestamp understood by a source-media decoder;
3. an instant measured from sequence zero;
4. an instant measured by the playback master clock;
5. elapsed sequence time versus elapsed master-clock time.

The final distinction matters whenever playback rate is not 1.0x. A duration of
one second on the playback clock represents two seconds of sequence progress at
2.0x. Giving both values the same C++ type would allow a plausible but incorrect
calculation to compile.

The independent reviews are recorded under
[`../reviews/`](../reviews/README.md).

## Decision

### Type set

Milestone 1 uses these strong value types:

```cpp
struct FrameRate;          // positive rational sequence rate

class TimelineFrame;       // nonnegative position on the sequence grid
class FrameCount;          // signed number of sequence frames

class SourceTimestamp;     // instant in source-media time
class SequenceTime;        // instant measured from sequence zero
class SequenceDuration;    // signed elapsed sequence time

class MasterClockTime;     // instant measured by IPlaybackClock
class ClockDuration;       // signed elapsed master-clock time
```

`PresentationTime` is retired. It previously combined sequence-relative time,
master-clock instants, and elapsed durations under one name.

Each class keeps its underlying numeric storage private. Construction from and
access to raw integers or `std::chrono` values is explicit and limited to named
conversion helpers, serialization code, and framework adapters. The exact
class layout is an implementation detail; the distinct types and operator
surface below are the contract.

`FrameRate` stores a numerator and denominator as 32-bit integers. Construction
requires both to be greater than zero and normalizes the fraction. Equality is
rational equality, so `30/1` equals `60/2`; implementations use 64-bit
intermediates when comparing products.

Every in-memory `TimelineSequence` carries a `FrameRate`. Milestone 1
synthesizes one sequence at `30/1` when loading the existing flat project
format. The tested 24, 25, 30, and 30000/1001 rates exercise conversion helpers;
they do not add frame-rate UI or change persisted projects.

### Permitted arithmetic

Only these same-domain operations are provided:

```text
TimelineFrame - TimelineFrame  -> FrameCount
TimelineFrame +/- FrameCount   -> TimelineFrame
FrameCount +/- FrameCount      -> FrameCount

SequenceTime - SequenceTime           -> SequenceDuration
SequenceTime +/- SequenceDuration     -> SequenceTime
SequenceDuration +/- SequenceDuration -> SequenceDuration

MasterClockTime - MasterClockTime -> ClockDuration
MasterClockTime +/- ClockDuration -> MasterClockTime
ClockDuration +/- ClockDuration   -> ClockDuration
```

Equality and relational comparison (`==`, `!=`, `<`, `<=`, `>`, and `>=`) are
provided between two values of the same type, including rational comparison of
two `FrameRate` values. Comparison across different types or time origins is
ill-formed.

The following named constants make origin and neutral values explicit:

```cpp
TimelineFrame::zero();
FrameCount::zero();
SequenceTime::zero();
SequenceDuration::zero();
ClockDuration::zero();
```

Unary negation is provided for the signed difference types `FrameCount`,
`SequenceDuration`, and `ClockDuration`.

Constructing a `TimelineFrame` or `SequenceTime` below sequence zero violates a
precondition. The caller must make any clamping policy explicit before the
operation. Signed `FrameCount`, `SequenceDuration`, and `ClockDuration` values
remain legal because differences may be negative.

Everything not listed is ill-formed, including:

- adding two positions or two instants;
- assigning or comparing values from different time origins;
- multiplying a `TimelineFrame` by playback rate;
- adding `ClockDuration` directly to `SequenceTime`;
- adding `SequenceDuration` directly to `MasterClockTime`.

The separate duration types are deliberate. A single shared duration type
would let rate-dependent sequence/clock mistakes compile.

### Named domain conversions

Domain changes happen only through named functions:

```cpp
SequenceTime sequenceTimeAtFrameStart(
    TimelineFrame frame,
    FrameRate sequenceRate);

SequenceTime sequenceTimeAtNextFrameStart(
    TimelineFrame frame,
    FrameRate sequenceRate);

TimelineFrame frameAtSequenceTime(
    SequenceTime time,
    FrameRate sequenceRate);  // precondition: time >= sequence zero

struct ClipTimeMapping {
    TimelineFrame clipStartFrame;
    std::optional<SourceTimestamp> sourceIn;
    FrameRate sequenceRate;
};

std::optional<SourceTimestamp> sourceTimestampFor(
    TimelineFrame frame,
    const ClipTimeMapping &mapping);

SequenceDuration sequenceElapsedFor(
    ClockDuration elapsed,
    int playbackRatePercent);

ClockDuration clockElapsedFor(
    SequenceDuration elapsed,
    int playbackRatePercent);
```

`playbackRatePercent` is positive and dimensionless. The last two functions are
the only sequence-duration/master-clock-duration bridges. Rate never changes a
stored timeline position, clip duration, or source trim value.

Still images have no source timestamp. Their resolved mapping therefore carries
`std::nullopt` rather than timestamp zero, which remains a valid timestamp for
video and audio.

### Boundary and rounding policy

`sequenceTimeAtFrameStart()` returns the start of the requested frame.
`sequenceTimeAtNextFrameStart()` makes the half-open interval
`[frameStart, nextFrameStart)` explicit and testable.

`frameAtSequenceTime()` uses mathematical floor semantics. It accepts only
nonnegative sequence time; callers clamp a negative intermediate value before
calling it. This avoids depending on C++ signed integer division, which
truncates toward zero.

Frame/time conversion is calculated from the absolute value. The engine never
advances by repeatedly adding a rounded milliseconds-per-frame value. The
implementation orders quotient and remainder operations so supported values do
not overflow 64-bit intermediates. Milestone 1 does not introduce custom
128-bit arithmetic; helpers document and test a supported range of at least 24
hours at every required rate.

Playback-rate helpers round signed sub-microsecond results toward zero. They are
not promised to be exact inverses after integer rounding. Scheduler re-anchoring
therefore retains its exact `(MasterClockTime, SequenceTime)` anchor instead of
reconstructing either side by round-tripping through both rate helpers; the
anchor lifecycle policy belongs to ADR-004.

Timecode is formatted from `TimelineFrame`, not from a rounded timestamp.

### Source mapping and seek scope

For the existing project format, `sourceInFrame` denotes fixed 30/1-grid units:

```text
legacy source time = sourceInFrame / 30 seconds
```

This preserves the behavior of the current player, which converts the field to
milliseconds using the same fixed 30 fps assumption. It does not claim that the
value is an index in the source file's encoded frame rate.

The snapshot builder converts the authoritative legacy field one way into a
`SourceTimestamp`. Editing and saving continue to use the original frame field;
derived snapshot timestamps are never converted back and written to the
project. The project format therefore remains unchanged and repeated load/save
cycles cannot introduce timestamp-rounding drift.

Real source frame rate, variable-frame-rate metadata, container time bases, and
sample-exact source timing are deferred to a source-media metadata ADR. Until
then, a source is sampled at sequence instants and its decoder may repeat or
drop frames. ADR-001 defines the requested `SourceTimestamp`; the concrete
decoder's keyframe seeking and returned-frame selection policy belongs to a
later decoder/seek contract.

### Master-clock scope for milestone 1

Milestone 1 uses an `IPlaybackClock` backed by
`std::chrono::steady_clock`, even when audible audio is active.
`QMediaPlayer::position()` is an observation from the Qt adapter, not the
authoritative engine clock.

The current `QMediaPlayer` plus `QAudioOutput` path owns audio playback outside
the new scheduler's clock loop. Milestone 1 verifies concurrent V1/A1 playback
and lifecycle behavior but does not claim a measured A/V synchronization
tolerance. Audio-device clocking, `AudioSampleCount`, `SampleRate`,
`QAudioSink`, drift correction, and sample-exact synchronization are deferred
to the master-clock ADR.

`MasterClockTime` is not tied by name or representation to `steady_clock`.
`IPlaybackClock` may later supply an audio-derived clock without changing the
session and scheduler APIs.

### Framework boundaries

Qt-specific units remain adapter details:

- `QMediaPlayer` millisecond positions are converted at the Qt playback
  adapter;
- `QVideoFrame` microsecond timestamps become `SourceTimestamp` at the video
  adapter;
- UI sliders and timecode convert to or from `TimelineFrame` at the UI
  boundary;
- no Qt unit appears in the framework-neutral timeline or playback APIs.

## Why this decision

The primary benefit is semantic safety. The compiler rejects a timeline
position passed directly to a decoder, a sequence-relative instant used as a
clock deadline, or a clock duration added to sequence time without playback-rate
conversion.

The representation also fits each job:

- editing remains exact and readable on the sequence frame grid;
- decoder requests use source timestamps instead of assuming a real source
  frame rate;
- scheduling distinguishes sequence progress from elapsed master-clock time;
- clock implementation can change later without changing engine contracts.

## Consequences

Positive consequences:

- domain and origin mistakes become compiler errors;
- 29.97 and other rational sequence rates can be tested safely;
- source trim mapping becomes an explicit snapshot/resolver responsibility;
- playback-rate conversion has a single visible boundary;
- Qt unit conversions remain isolated in adapters.

Costs and limitations:

- call sites require explicit construction and conversion functions;
- serialization and legacy frame fields need migration adapters;
- arithmetic helpers require boundary, precondition, and long-duration tests;
- milestone 1 does not provide measured A/V synchronization;
- the existing project format cannot support a non-30 sequence rate. Persisting
  `FrameRate` is a hard precondition before another sequence rate is exposed;
- microseconds do not exactly represent every media time base. Source metadata
  may later adopt a richer source-time representation without weakening these
  domain boundaries.

## Alternatives considered

### Use one integer frame number everywhere

Rejected because source media and sequences may have different rates, and
variable-frame-rate sources are timestamp-oriented.

### Use one microsecond timestamp everywhere

Rejected because timeline editing needs an exact frame grid and because
sequence-relative and master-clock instants have different origins.

### Use one shared duration type

Rejected for milestone 1. It would allow a `ClockDuration` to be added directly
to `SequenceTime`, bypassing playback-rate conversion.

### Use double seconds everywhere

Rejected because equality and boundary decisions become vulnerable to
floating-point rounding, while domains and origins remain indistinguishable.

### Introduce a fully generic rational-time class immediately

Deferred. It would improve exact source time-base representation but expands
the first milestone. Strong domain types allow `SourceTimestamp` to evolve
later without changing ownership contracts.

## Acceptance criteria

ADR-001 is implemented when:

1. Review of the playback engine's public headers confirms that no API exposes
   unqualified integer frame or time parameters.
2. Compile-time tests prove the strong types are not implicitly convertible and
   unsupported cross-domain arithmetic is unavailable.
3. `FrameRate` rejects nonpositive values, normalizes fractions, and compares
   rationally equal rates as equal.
4. Conversion helpers pass tests for 24, 25, 30, and 30000/1001.
5. Frame-start, next-frame-start, half-open boundary, and floor tests pass;
   debug preconditions reject negative `SequenceTime`, and caller tests prove
   that negative intermediate values are explicitly clamped before conversion.
6. Long-duration tests of at least 24 hours prove rounding is calculated from
   absolute values and does not accumulate per frame.
7. Playback-rate tests prove `ClockDuration` and `SequenceDuration` cross only
   through the named rate helpers.
8. A still-image mapping returns no `SourceTimestamp`; zero remains valid for
   video and audio.
9. Qt millisecond and video-frame timestamp conversions exist only in Qt
   adapters.
10. Existing frame fields remain authoritative through load, snapshot build,
    edit, and save; derived timestamps are not persisted back into them.
11. Milestone 1 creates an in-memory 30/1 sequence without changing the project
    format or visible frame-rate UI.
12. The real V1/A1 integration scenario tests concurrent playback, seek,
    pause/resume, and shutdown, but does not assert sample-exact A/V sync.

## Learning focus

When reviewing code under this ADR, ask three questions:

1. Does the value belong to the correct domain?
2. Does an instant use the correct origin?
3. Is every domain or rate conversion visible at the call site?
