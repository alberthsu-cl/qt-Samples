# ADR-004: Audio/Monotonic Master-Clock Policy

Status: Proposed

Date: 2026-09-02

## Context

ADR-002 makes `PlaybackSession` the sole transport authority. ADR-003 makes
playback consume immutable snapshots and rejects stale asynchronous work by
generation. The remaining timing decision is how a session turns elapsed time
into a sequence position when audio and video are both active.

The current Qt path reports `QMediaPlayer` position and uses a UI heartbeat.
That is useful for the migration bridge, but it does not define a stable
engine clock: repaint frequency, decoder latency, and video presentation must
not advance transport time. A/V playback needs one explicit clock authority,
deterministic re-anchoring, and a policy for early or late video frames.

## Decision

`PlaybackSession` owns a clock abstraction. The scheduler derives sequence
position from an anchor and the current master-clock reading; it never advances
position by counting UI timer callbacks or by adding rounded frame durations.

The master-clock policy is:

1. When audible audio is active, the audio-output clock is authoritative.
2. Otherwise, a monotonic clock is authoritative.
3. Milestone 1 uses `std::chrono::steady_clock` for both cases because the
   existing Qt media bridge does not yet expose a sample-accurate device clock.
4. Replacing the milestone clock with an audio-device clock does not change
   `MasterClockTime` or scheduler ownership; it changes only the clock adapter.

The UI may sample published `PlaybackStatus` for painting, but it never advances
the clock or transport position.

This ADR's anchor and re-anchoring machinery governs `SequencePreview`, where
position is derived from a sequence snapshot and its frame rate. In milestone
1, `SourceAssetPreview` continues to adopt position from backend observations
as specified by ADR-002 and does not consume `IPlaybackClock`. A future decision
may unify those paths without changing the ownership boundary; that unification
is outside this ADR.

## Clock contract

The clock is injected so engine tests can use a deterministic fake:

```cpp
class IPlaybackClock {
public:
    virtual ~IPlaybackClock() = default;
    virtual MasterClockTime now() const = 0;
};
```

`MasterClockTime` and `ClockDuration` are the strong types established by
ADR-001. Implementations may use `steady_clock` or an audio-device position;
the engine does not depend on either implementation detail.

The scheduler keeps one immutable anchor for the current transport epoch:

```cpp
struct PlaybackAnchor {
    MasterClockTime masterClock;
    SequenceTime sequenceTime;
    int playbackRatePercent;
};
```

`playbackRatePercent` is captured from the session's rate preference whenever a
new anchor is created. It is not an independently mutable clock value.

For a playing session:

```text
elapsedClock = clock.now() - anchor.masterClock
elapsedSequence = sequenceElapsedFor(elapsedClock, anchor.playbackRatePercent)
sequenceTime = anchor.sequenceTime + elapsedSequence
timelineFrame = frameAtSequenceTime(sequenceTime, snapshot.frameRate)
```

All conversions are named functions. Rational frame-rate arithmetic and
rounding follow ADR-001; the scheduler does not accumulate rounded frame
durations.

## Re-anchoring rules

The scheduler replaces the anchor atomically at these boundaries:

- entering `Playing` after `Prerolling` or `Paused`;
- a seek completion;
- a playback-rate change;
- selecting or replacing the master clock;
- installing a new sequence snapshot;
- resuming after an audio underflow or device restart.

The new anchor records the current resolved sequence position and the current
clock reading. Re-anchoring never changes the `PlaybackSession` authority or
creates a second transport state. Seek, source replacement, snapshot
replacement, stop, and natural completion still advance the generation under
ADR-002/ADR-003.

Pause evaluates the anchor equation once at the pause instant, captures that
resolved sequence position, and holds it fixed; no later elapsed clock time is
converted while paused. Stop invalidates pending work and returns
to the defined start position. Natural completion publishes the final position,
invalidates the generation, and enters the existing stopped/completed policy;
restarting begins from the defined start position.

## Audio-master and video policy

For this policy, audible audio is active when the session has an enabled audio
track with non-zero output volume and the audio device is open for playback.
When audible audio is active, audio submission and the audio clock are not
blocked by video decode or presentation. Video follows the target sequence
time:

- retain a small bounded queue of timestamped decoded video frames;
- wait briefly when the next frame is early;
- present the newest frame not later than the target time;
- drop superseded late frames;
- report a **video-frame underflow** without moving the master clock when no
  suitable frame is available.

An **audio-buffer underflow** means that the audio callback has insufficient
queued samples. Its recovery behavior (silence, hold, device restart, or other
policy) is a separate implementation decision; it must not let the UI advance
transport time. The exact audio buffer depth, device-latency measurement, and drift thresholds
are implementation parameters of this ADR's scheduler policy and must be
covered by tests. The audio callback never waits for a video frame.

When no audible audio is active, the monotonic clock follows the same anchor
equations and video policy. Enabling or disabling audible audio is a clock
selection boundary and therefore re-anchors before more work is scheduled.

## Identity and thread boundaries

Every scheduled decode/composition request derived from a clock reading carries
the active `PlaybackSessionId`, `PlaybackGeneration`, and sequence snapshot
identity from ADR-003. A synchronous `clock.now()` read is engine-local and
does not need an identity of its own. A clock change or re-anchor cannot make an
old result current.

The engine thread owns phase, anchor, generation, and clock selection. Audio
callbacks and decoder workers publish observations or completed work; they do
not mutate the anchor, phase, snapshot, or UI objects. The audio callback must
not block, allocate, or contend on locks with the engine thread; it hands off
observations through a lock-free or otherwise non-blocking path. Command-queue ownership,
worker lifetime, and callback shutdown belong to ADR-005.

## Acceptance criteria

ADR-004 is implemented when:

1. Playback position is derived from an injected `IPlaybackClock` and an
   anchor, never from UI callback count.
2. The clock-selection logic runs when audible audio becomes active or
   inactive, and each transition triggers exactly one re-anchor; milestone 1
   may use the same `steady_clock` implementation for both branches.
3. Tests use a fake clock to prove deterministic position conversion at 24,
   25, 30, and 30000/1001 frame rates through ADR-001 helpers.
4. Pause freezes sequence time; resume re-anchors without a discontinuity.
5. Seek, rate change, snapshot replacement, clock replacement, audio restart,
   stop, and natural completion re-anchor or invalidate work as specified.
6. Video early/late/video-frame-underflow behavior is bounded and does not block the audio
   callback or advance transport from the UI thread.
7. Stale results after a clock change or re-anchor are rejected by the existing
   session/generation/snapshot identity checks.
8. A/V policy tests prove that video follows the master clock and audio never
   waits for video.
9. A test or code-level ownership check proves that the Qt bridge publishes
   clock observations and never calls a playback-position or phase mutator on
   `PlaybackSession`.

## Consequences

Playback remains stable across UI stalls and decoder timing variation. Audio
can become the authoritative clock without changing the framework-neutral time
types or presentation contract. The design introduces explicit scheduler and
clock seams that ADR-005 can connect to Qt audio callbacks, decoder workers, and
shutdown sequencing.

The milestone-1 steady-clock exception remains intentionally less precise than
the target audio-device clock. It is observable and testable, rather than an
implicit behavior hidden in `QMediaPlayer`.

## Deferred decisions

ADR-005 defines engine/decoder/audio-callback/UI thread ownership and shutdown.
Source-media time bases and variable-frame-rate metadata remain outside this
decision. Device-specific latency measurement, resampling, audio-buffer-
underflow recovery behavior, and a complete audio drift-correction algorithm
may require a follow-up implementation ADR if the scheduler tests expose
requirements not settled here.
