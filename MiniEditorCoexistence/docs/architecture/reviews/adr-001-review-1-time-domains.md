# ADR-001 Review 1 — Strong Media Time Domains

Verdict: **Accept with revisions**

Round: 1 (original review)

Reviewed against [`../current-playback-architecture.md`](../current-playback-architecture.md),
[`../target-playback-architecture.md`](../target-playback-architecture.md), and
the code the migration must carry: `QtMediaPlaybackBackend`, `MediaLibrary`,
`TimelinePlaybackResolver`, and `ProjectSerializer` v9.

Superseded in part by [review 2](adr-001-review-2-resolutions.md), which records
the owner's resolutions and one correction to this document.

## Verdict

The core decision is correct and well argued: three distinct domains, no
implicit conversion, a rational `FrameRate` per sequence, conversion from the
absolute value rather than accumulated per-frame rounding, and Qt units confined
to adapters. The alternatives section correctly rejects single-int64 frames,
single-microsecond time, and doubles.

Revisions are needed because the ADR under-specifies in four places where the
compiler will not help, and in one of them it reintroduces the exact ambiguity
it exists to eliminate. None of the fixes expand milestone scope; most are
wording and acceptance-criteria changes.

Totals: 5 blocking, 9 non-blocking, 6 open questions, 13 recommended edits.

## Blocking issues

### B1 — PresentationTime has three origins and one type

The most serious finding, because ADR-001's entire justification is that a
value's meaning must live in its type.

The Decision section defines it as "elapsed playback-clock time". But three
different origins already share the name:

| Producer | Origin |
| --- | --- |
| `presentationTimeForFrame()` | sequence zero |
| `VideoDecodeRequest::deadline` (target Decision 5) | clock epoch |
| `anchor + elapsed * rate` (target Decision 6) | anchor — a duration, not an instant |

Two values of one type that must never be subtracted, yet subtraction compiles.
A `deadline` computed from `presentationTimeForFrame()` without adding the
anchor is a plausible, silent, sync-destroying bug.

**Fix.** Split the type: `SequenceTime` (sequence-relative instant, produced
from `TimelineFrame`) and `MasterClockTime` (monotonic instant), with a duration
type between them. Or keep one name but parameterise it on a clock tag so the
origins cannot mix.

### B2 — No audio-sample domain, yet audio is the designated master clock

Target Decision 6 makes the audio-output clock the master whenever audible audio
is active. Audio device position is natively a sample count; at 48 kHz one
sample is 20.833 us and is not representable in microseconds.

Numerically this is harmless — A/V sync tolerance is tens of milliseconds, so
microseconds are ample. The problem is *domain*, which is what this ADR is for:
nothing stops a sample index being passed where microseconds are expected. The
engine has four domains; the ADR names three.

There is also a factual gap. Milestone 1's audio path is `QMediaPlayer` plus
`QAudioOutput`. **`QAudioOutput` exposes no clock at all** — the only position
available is `QMediaPlayer::position()`, in milliseconds. An audio master clock
at microsecond resolution needs `QAudioSink` and an owned PCM path, which is not
in this milestone.

**Fix.** Either add `AudioSampleCount` and `SampleRate` as a fourth domain, or
scope it out explicitly and state the consequence. The ADR must not imply
end-to-end microsecond precision the stack cannot deliver.

### B3 — Positions and durations share one type

`std::chrono::microseconds` is a *duration* type, used here for two *instants*.
Target Decision 4 then declares `TimelineFrame duration` — a position type used
as a length.

These compile today and are meaningless:

```text
SourceTimestamp + SourceTimestamp     two instants added
TimelineFrame{ a.value + b.value }    two positions added
duration * scalar   // on a position  scaled an instant
```

The Decision section promises "no implicit conversions ... to their underlying
integer representation", but the structs shown carry a public `.value`, which is
an *explicit* escape hatch. With no operator surface defined, every call site
reaches for it and the guarantee evaporates in practice.

**Fix.** Define the permitted operators normatively, and give each domain
distinct position and duration types.

### B4 — Floor semantics asserted but undefined for negatives

The Decision section says "Timeline-to-frame conversion uses floor semantics".
Two defects:

1. The sentence is mislabeled. It describes *time-to-frame*.
2. C++ integer division truncates toward zero, so for negative inputs `/` yields
   ceil, not floor. A naive implementation silently violates the stated contract
   on one side of zero.

Acceptance criterion 5 requires floor-semantics tests but never requires
negative inputs; negatives appear only in Consequences as a cost. The ADR also
never says which domains admit negative values. The scheduler needs at least one
of them to: target Decision 6 says "wait when the next video frame is early".

**Fix.** Mandate an explicit floor-division helper, state legality per domain,
and add negative inputs to the acceptance criteria.

### B5 — sourceInFrame migration is ambiguous, and the save path is unspecified

The Decision section says the snapshot builder converts using "the currently
known source/sequence rate". Source rate and sequence rate are different
quantities and the slash hides the choice.

Acceptance criterion 8 covers *loading* only. The application also *saves* —
`ProjectSerializer` v9 writes `sourceInFrame` as an int. A load, edit, save cycle
may walk a trim point by one source frame each round. That is data fidelity, not
style.

Two adjacent conflations, neither mentioned:

- `MediaLibrary::timelineDurationFrames` stores source media duration expressed
  in timeline frames at 30 fps.
- `kDefaultAudioDurationFrames` and the 90-frame still default become
  rate-dependent once `FrameRate` is real. A 90-frame still is 3 s at 30 fps and
  3.75 s at 24 fps.

Evidence that the existing millisecond boundary already loses frames:

```text
positionMillisecondsForFrame(1)   = 1 * 1000 / 30   -> 33 ms
unclampedPlayerPositionFrame(33)  = 33 * 30 / 1000  -> frame 0
```

The same class also rounds duration with `+500` but truncates position.

**Fix.** Name one legacy rule and state the save-side decision explicitly.

## Non-blocking improvements

| ID | Improvement |
| --- | --- |
| N1 | `FrameRate` validity is undefined. Specify positive numerator and denominator, and either normalise on construction or define equality by cross-multiplication. Is 30/1 equal to 60/2? |
| N2 | Still images have no source time. "No source time" should be *absent*, not zero. Zero is a real timestamp and will be read as one. |
| N3 | Document that `presentationTimeForFrame` returns the frame's *start*, and add a companion returning the next frame's start so the half-open interval is testable. |
| N4 | Define frame-selection semantics for seek: decode from the last keyframe at or before the target; present the frame with the greatest PTS <= target. Without it, VFR behaviour is undefined and the trimmed-seek integration test is not reproducible. |
| N5 | State that no retiming occurs when source rate differs from sequence rate. The clip samples the source at sequence instants; frames repeat or drop. |
| N6 | `ResolvedClipTimeMapping` is referenced in the example boundary functions but defined nowhere. |
| N7 | Overflow guidance is over-specified. With int64 frames and microseconds the straightforward form is safe past ~9e9 frames, about 9.7 years at 29.97. A hand-rolled 128-bit helper adds its own bug surface for a threat the milestone cannot reach. MSVC C++17 x64 has no `__int128`; you would need `_mul128`/`_udiv128`. |
| N8 | Re-anchoring must be exact. Criterion 6 forbids accumulated per-frame rounding, but pause, seek and rate-change re-anchoring reintroduce drift if the new anchor is recomputed from a rounded time. |
| N9 | `ratePercent` is dimensionless and scales the clock domain only. It must never multiply a `TimelineFrame`. |

## Questions raised

1. Is the master clock in milestone 1 actually the audio device?
2. Are negative `TimelineFrame` values legal?
3. Does the project format change in this milestone?
4. Is the *source* frame rate modelled at all?
5. Is `PresentationTime` anchor-relative or monotonic-absolute?
6. Do the four required test rates imply sequence rate becomes user-visible?

All six were answered by the owner; see
[review 2](adr-001-review-2-resolutions.md).

## Recommended edits

| # | ADR-001 section | Change | Ref |
| --- | --- | --- | --- |
| 1 | Decision, types | Split `PresentationTime` into `SequenceTime` and `MasterClockTime`, with a duration type between. | B1 |
| 2 | Decision, types | Add position/duration distinction per domain; make the microsecond-based types instants, not raw `microseconds`. | B3 |
| 3 | Decision, after types | Add a normative operator surface; state that `.value` is an adapter-boundary accessor. | B3 |
| 4 | Decision, floor semantics | Retitle to "time-to-frame"; mandate a floor-division helper; state per-domain legality of negatives. | B4 |
| 5 | Decision, rational conversion | Replace the bespoke overflow mandate with a stated safe range, fixed multiplication order, and precondition assert. | N7 |
| 6 | Decision, Qt units | Add that `QAudioOutput` exposes no clock, and the only audio position is `QMediaPlayer::position()` in ms. | B2 |
| 7 | Decision, migration | Replace "source/sequence rate" with one named rule; state the save-side decision. | B5 |
| 8 | Decision, new subsection | Audio timing: a fourth domain, or an explicit scope-out with the consequence stated. | B2 |
| 9 | Example boundary functions | Define `ResolvedClipTimeMapping`; document the returned instant; add the frame-end companion. | N3, N6 |
| 10 | Acceptance criteria | Add negative-input and floor-division tests; round-trip load/save fidelity; `FrameRate` validity and equality tests; a compile-fail test proving cross-domain assignment does not build. | B3, B4 |
| 11 | Acceptance criteria 8 | Extend from "can still be loaded" to "loaded *and saved* without drift across N cycles". | B5 |
| 12 | Consequences, costs | Add `MediaLibrary::timelineDurationFrames` and the 90-frame still default as known rate-dependency costs. | B5 |
| 13 | Learning focus | Extend from "correct domain" to "correct domain *and correct origin*". B1 is exactly the failure this section is meant to prevent. | B1 |

## Scope judgement

**Under-designed:** the audio domain, the position/duration split,
`PresentationTime`'s origin, seek and frame-selection semantics, and the
save-side migration. All five are cheap to fix in the document and expensive to
fix in code.

**Over-designed:** the bespoke overflow-aware arithmetic mandate. That is the one
place the ADR reaches past what the milestone needs.

**Right-sized:** strong domain types, per-sequence rational `FrameRate`,
absolute-value conversion, and Qt units in adapters. Testing 24, 25, 30 and
30000/1001 costs almost nothing and is the whole point. Keep it, and do not add
sequence-rate UI to justify it.
