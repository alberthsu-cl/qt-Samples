# ADR-001 Review 2 — Owner Resolutions

Round: 2 (resolution review)

Scope: the owner's six resolutions only. ADR-001 is not rewritten here.

Follows [review 1](adr-001-review-1-time-domains.md), whose verdict of **Accept
with revisions** is carried forward.

## Position

**Four of five blocking findings are closed.** B1, B3 and B5 are resolved; B4 is
resolved apart from one contract hole; B2 is a defensible deferral. What remains
is three document-level amendments and one decision — none of which blocks the
type set in the section below, and none of which grows the milestone.

B5's resolution is better than what review 1 recommended. Keeping frames
authoritative in the editable model and converting one-way at snapshot build
removes the round trip entirely, so round-trip drift is designed out rather than
tested for.

## Resolutions as given

1. Milestone 1 uses `std::chrono::steady_clock` as its master clock.
   `QMediaPlayer::position()` is only a playback observation. A true
   audio-device master clock and sample-exact audio timing are deferred.
2. Public `TimelineFrame` positions are nonnegative. Signed differences may
   still represent early/late scheduling deltas.
3. The project persistence format will not change in this milestone. Existing
   frame fields remain authoritative in the editable project model. Timestamp
   conversion is one-way when constructing an immutable playback snapshot, so
   load/save cycles do not convert timestamps back into frames.
4. Real source frame rate, variable-frame-rate metadata, and container time
   bases are deferred to a source-media metadata ADR. Legacy `sourceInFrame`
   uses the existing fixed 30/1 interpretation.
5. Sequence-relative time and monotonic master-clock time become separate strong
   types.
6. Tests for 24, 25, 30 and 30000/1001 validate conversion helpers only. The
   initial editor UI and sequence remain fixed at 30/1.

B2 is accepted as a scope warning, with `AudioSampleCount`, `SampleRate`,
`QAudioSink` integration and sample-exact synchronisation deferred.

## 1. Status of each blocking finding

| ID | Finding | Status | Note |
| --- | --- | --- | --- |
| B1 | Separate time origins | Resolved | R5 gives two strong instant types. One follow-through in section 3. |
| B2 | Audio-sample domain | Deferred | Sound for the type set. Creates two documentation obligations, not code. |
| B3 | Positions vs durations | Resolved | In principle. The operator table is the actual deliverable — see section 4. |
| B4 | Negatives and floor division | Partial | R2 settles domain legality. The conversion contract at negative input is still open. |
| B5 | Migration and persistence | Resolved | One-way conversion at snapshot build removes the round trip entirely. |

### Correction to review 1

Review 1 flagged that reinterpreting `sourceInFrame` as 1/30 s units rather than
a source frame index would change behaviour on existing projects. **It does
not.** The current code already does exactly that — `positionMillisecondsForFrame()`
computes `frame * 1000 / 30` and feeds it to `QMediaPlayer::setPosition()`.

R4's fixed 30/1 interpretation is therefore faithful to today's behaviour,
including for 25 fps and 29.97 fps source files. That risk is withdrawn.

R3 also neutralises the rest of B5 for this milestone. With nothing
reinterpreting frames at a second rate, `MediaLibrary::timelineDurationFrames`,
the 90-frame still default and the audio default all stay correct as written.

## 2. Remaining blocking issues

None of these blocks ADR-001's type set. Two are target-document amendments; one
is a contract hole.

### RB1 — Milestone 1 fails target invariant 7 as written

Invariant 7 reads "Audio is the master clock when audible audio is active;
otherwise a monotonic clock is used", and target Decision 6 says the same. R1
says `steady_clock` always. The invariants are explicitly "intended to become
acceptance criteria and tests", so as written the milestone cannot pass its own
acceptance list.

**Fix.** Amend invariant 7 with a milestone qualifier, or restate it as a
target-state invariant carrying a dated exception.

### RB2 — frameAtSequenceTime() has no defined contract at negative input

R2 makes public `TimelineFrame` positions nonnegative, which is right. But the
conversion still receives times that can go negative — a frame early relative to
the anchor, or a seek computed below zero before clamping. Three options:
precondition and assert, clamp at zero, or return a signed `FrameCount` offset
and make the caller construct the position.

**Recommend.** Precondition plus assert, with clamping done explicitly by the
caller, so the clamp is visible where the policy actually lives.

### RB3 — A/V sync has no closed loop and no test criterion

With `steady_clock` as master and A1 audio self-clocking inside its own
`QMediaPlayer`, nothing corrects drift between them. That matches today's
behaviour, so it is not a regression — but the target's integration list includes
"one real V1 video plus one real A1 audio" with no stated tolerance.

**Fix.** Give it an observational tolerance, or state plainly that milestone 1
does not assert A/V sync. Otherwise it is an acceptance item nobody can pass or
fail.

## 3. Contradictions with the target architecture

1. **Invariant 7 and Decision 6 versus steady_clock-only** — RB1 above. The
   substantive one.
2. **Decision 1's sequence-ready project versus "persistence will not change".**
   Decision 1 restructures `EditorProject` to hold
   `std::vector<TimelineSequence>`, each carrying a `FrameRate`. R3 freezes the
   format. These can coexist — introduce `TimelineSequence` *in memory only*,
   synthesise one default 30/1 sequence on load, and keep the serializer writing
   the flat `timelineItems` — but that has to be said, or the first implementer
   will assume the format changes.
3. **`VideoDecodeRequest::deadline` is typed `PresentationTime`** in target
   Decision 5. Under R5 that type is retired; the field must become
   `MasterClockTime`. Small, concrete, easy to miss.
4. **Latent, not yet a contradiction.** With the format frozen, a project file
   carries no sequence frame rate. Harmless while everything is 30/1, but the
   moment a second rate ships, a loaded project silently reinterprets every clip
   time. Record it as a hard precondition on future rate work.

## 4. Minimal type set for milestone 1

Six types, one alias, five functions. `PresentationTime` is **retired** — say so
explicitly, so it does not survive as a third thing alongside the two that
replace it.

```cpp
// Rate ------------------------------------------------------------
struct FrameRate {           // numerator > 0, denominator > 0
    std::int32_t numerator   = 30;
    std::int32_t denominator = 1;
};                           // equality by cross-multiplication

// Timeline domain -------------------------------------------------
struct TimelineFrame { std::int64_t value = 0; };  // position, nonnegative
struct FrameCount    { std::int64_t value = 0; };  // signed difference

// Source domain ---------------------------------------------------
struct SourceTimestamp { std::chrono::microseconds value{}; };  // instant

// Instants, distinguished by origin -------------------------------
struct SequenceTime    { std::chrono::microseconds value{}; };  // origin: sequence zero
struct MasterClockTime { std::chrono::microseconds value{}; };  // origin: clock epoch

using Duration = std::chrono::microseconds;        // signed delta of any instant
```

Permitted operators. Everything not listed is ill-formed:

```text
TimelineFrame  -  TimelineFrame   -> FrameCount
TimelineFrame  +- FrameCount      -> TimelineFrame      (asserts nonnegative)
FrameCount     +- FrameCount      -> FrameCount
<Instant>      -  <Instant>       -> Duration           (same type only)
<Instant>      +- Duration        -> <Instant>
Duration       +- Duration        -> Duration

ill-formed: instant + instant, any cross-domain assignment,
            TimelineFrame * scalar, Duration compared across origins
```

The only functions allowed to cross a domain:

```cpp
SequenceTime    sequenceTimeAtFrameStart(TimelineFrame, FrameRate);
TimelineFrame   frameAtSequenceTime(SequenceTime, FrameRate);   // floor; precondition >= 0
SourceTimestamp sourceTimestampFor(TimelineFrame, const ClipTimeMapping&);
Duration        scaleByRate(Duration, int ratePercent);         // only sequence<->clock bridge

std::optional<SourceTimestamp>   // absent for stills — N2

struct ClipTimeMapping {         // this is N6's missing type
    TimelineFrame   clipStartFrame;
    SourceTimestamp sourceIn;
    FrameRate       sequenceRate;
};
```

### One deliberate weakening — choose it knowingly

`Duration` is shared between the sequence and clock domains. Adding a clock
duration to a `SequenceTime` is correct at 100% rate and wrong at 150%, and this
type set will not catch it. `scaleByRate()` being the sole bridge is the
discipline instead.

Separating `SequenceDuration` from `ClockDuration` closes the hole, at the cost
of two more types and a lot of call-site noise. For one sequence and four fixed
rates, take the weaker version and revisit only if rate-change bugs appear.

### Two naming notes

Keep ADR-001's existing `SourceTimestamp` rather than renaming, to hold churn
down. And do **not** bake `steady_clock` into `MasterClockTime`'s name or
definition — keep the clock behind `IPlaybackClock` so ADR-004 can substitute an
audio clock without touching the type.

## 5. Fold into ADR-001 now

| ID | Change |
| --- | --- |
| N1 | `FrameRate` validity, normalisation, equality — it is part of the type set. |
| N2 | Still images carry no source time; `optional`, not zero. |
| N3 | Which instant is returned — largely solved by naming it `sequenceTimeAtFrameStart`. Add the frame-end companion so the half-open interval stays testable. |
| N4 | Seek and frame-selection semantics. **Promoted by R4:** with source metadata deferred, "last keyframe at or before the target; present the frame with greatest PTS <= target" is the only thing that makes trimmed-source seek testable. |
| N5 | No retiming when source rate differs from sequence rate — plus the sentence that makes R4 safe: `sourceInFrame` denotes 1/30 s units, not a source frame index. |
| N6 | `ClipTimeMapping` — defined in section 4. |
| N7 | Replace the overflow mandate with the stated safe range. This *removes* implementation work. |
| N9 | `ratePercent` is dimensionless and scales the clock domain only — now enforced by `scaleByRate()` being the sole bridge. |

Plus the four accepted blocking edits (B1, B3, B4, B5) and one sentence
recording the B2 deferral with its consequence.

## 6. Defer to later ADRs

| Item | Destination |
| --- | --- |
| `AudioSampleCount`, `SampleRate`, `QAudioSink`, sample-exact sync | Audio clock ADR, or ADR-004 |
| Audio-mastered clock selection, and RB1's invariant amendment | ADR-004 |
| N8 exact re-anchoring on pause, seek and rate change | ADR-004 — keep ADR-001's half: conversions are pure and stateless |
| Real source frame rate, VFR metadata, container time base | Source-media metadata ADR, per R4 |
| Separate `SequenceDuration` / `ClockDuration` | Only if rate-change bugs appear |
| Persisting the sequence `FrameRate` | **Hard precondition** for any second sequence rate |
| Snapshot construction and generation invalidation | ADR-003, as planned |
| Nested sequences and cycle detection | Already deferred; keep there |

## Bottom line

Land the section 4 type set and ADR-001 is implementation-ready without growing
milestone 1.
