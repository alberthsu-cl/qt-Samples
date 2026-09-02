# ADR-004 Review 2 — Resolution Verification

Verdict: **Accepted**

Round: 2 (resolution review, doubling as an acceptance check)

Revision reviewed: `86dc26f` *Revise ADR-004 after clock review*
(diff confined to ADR-004).

Reviewed against
[ADR-004](../decisions/0004-audio-monotonic-master-clock-policy.md) as it
stands after `86dc26f`, [review 1](adr-004-review-1-master-clock.md),
ADR-001, ADR-002, and ADR-003 (all three Accepted, closed), and
[`../target-playback-architecture.md`](../target-playback-architecture.md).

## Verdict

All eleven findings from review 1 — B1 and N1 through N10 — are resolved in
the applied revision, and several are resolved with more care than the
minimal fix called for: N3's audibility definition is concrete and grounded
in a property (`QAudioOutput::setVolume()`) that already exists in the
codebase rather than an invented one; N5 adds a cross-cutting guard ("it must
not let the UI advance transport time") beyond what was asked; N7's reworded
criterion is exactly the shape a fake-clock test needs.

No new blocking finding. One line-item worth a small follow-up, not a
blocker: the audibility definition uses the word "enabled" for an audio
track without tying it explicitly to the mix-affecting state ADR-003 already
established, leaving a hairline of room to misread it against the
view-only track-visibility toggle it must not mean. See N11.

Every diff line introduced by the revision was checked individually; nothing
outside the eleven findings' scope was altered, and no new contradiction,
incorrect type name, unclear ownership, untestable criterion, or scope
expansion was found.

## Resolution status

| ID | Resolution applied | Verified against text |
| --- | --- | --- |
| B1 | Scope paragraph added to the Decision section | "This ADR's anchor and re-anchoring machinery governs `SequencePreview` … In milestone 1, `SourceAssetPreview` continues to adopt position from backend observations as specified by ADR-002 and does not consume `IPlaybackClock`." Cites ADR-002 correctly and repeats ADR-002's own "future unification" language rather than inventing new scope. **Resolved.** |
| N1 | Derivation rule for `playbackRatePercent` | "`playbackRatePercent` is captured from the session's rate preference whenever a new anchor is created. It is not an independently mutable clock value." **Resolved.** |
| N2 | Pseudocode field name corrected | `sequenceElapsedFor(elapsedClock, anchor.playbackRatePercent)` now matches the struct field exactly. **Resolved.** |
| N3 | "Audible audio is active" defined | "…when the session has an enabled audio track with non-zero output volume and the audio device is open for playback." Grounded in an existing codebase concept (`QAudioOutput::setVolume()`), not invented. **Resolved**, with a small precision note — see N11. |
| N4 | "Underflow" disambiguated | "**video-frame underflow**" and "**audio-buffer underflow**" are now distinct, named, bolded terms used consistently at each site, including acceptance criterion 6's updated wording. **Resolved.** |
| N5 | Audio-underflow behavioral policy named as deferred | "Its recovery behavior (silence, hold, device restart, or other policy) is a separate implementation decision; it must not let the UI advance transport time." Added to the Deferred decisions list: "audio-buffer-underflow recovery behavior." **Resolved**, and the added UI-advancement guard is a genuine improvement over the minimal ask. |
| N6 | Audio-callback real-time-safety stated | "The audio callback must not block, allocate, or contend on locks with the engine thread; it hands off observations through a lock-free or otherwise non-blocking path." **Resolved.** |
| N7 | Acceptance criterion 2 reworded | "The clock-selection logic runs when audible audio becomes active or inactive, and each transition triggers exactly one re-anchor; milestone 1 may use the same `steady_clock` implementation for both branches." Tests the selection/re-anchor behavior directly, not a numeric difference that milestone 1 cannot produce. **Resolved.** |
| N8 | Acceptance criterion 9 reworded | "A test or code-level ownership check proves that the Qt bridge publishes clock observations and never calls a playback-position or phase mutator on `PlaybackSession`." Matches the ADR-002 criterion-13 model exactly, as recommended. **Resolved.** |
| N9 | Clock-sample identity scope narrowed | "Every scheduled decode/composition request derived from a clock reading carries the active … identity from ADR-003. A synchronous `clock.now()` read is engine-local and does not need an identity of its own." **Resolved.** |
| N10 | Pause-freeze capture mechanism described | "Pause evaluates the anchor equation once at the pause instant, captures that resolved sequence position, and holds it fixed; no later elapsed clock time is converted while paused." **Resolved.** |

## New checks for this round

**Contradictions.** None found, and the B1 fix is internally consistent with
the rest of the document rather than merely bolted on: the re-anchor trigger
list's asymmetry (a bullet for installing a new sequence snapshot, none for
opening a new source) is no longer a gap once source preview is excluded from
this ADR's machinery entirely — there is nothing left for a parallel bullet to
cover. The scoping decision is also well-motivated by the ADR's own Context
section, which frames the problem as synchronizing *independent* V1/A1
streams under one sequence clock; a single `QMediaPlayer` previewing one
source file has no such problem, since it keeps its own embedded audio and
video in sync internally. The exclusion is not an arbitrary carve-out — it is
the case the Context section was never actually describing.

**Incorrect type names.** None. Every strong type in the revised text —
`TimelineFrame`-family types via the arithmetic block, `MasterClockTime`,
`ClockDuration`, `SequenceTime`, `PlaybackAnchor`, `IPlaybackClock`,
`PlaybackSessionId`, `PlaybackGeneration`, `PlaybackSource`,
`SourceAssetPreview`, `SequencePreview`, `PlaybackStatus` — was checked
individually against ADR-001/002/003's accepted vocabulary. All match.

**Unclear ownership.** None beyond N11. "The engine thread owns … clock
selection" remains correct under the new audibility definition: mix-affecting
state arrives already resolved inside the active snapshot (ADR-003's
authority), and device-open/volume observations arrive as reported
observations from the audio adapter (this ADR's own "Audio callbacks …
publish observations" rule) — the engine thread only ever *decides* using
inputs it is already entitled to see. No new ownership question opened.

**Untestable acceptance criteria.** None. All nine criteria are now
objective and testable under MSVC C++17 with a fake clock and fake media
ports, closing the two review 1 flagged (2 and 9) without leaving any other
criterion weaker.

**Scope expansion.** None. Every addition in the diff is a clarification of
something already decided (in this ADR or in ADR-002/003) — no new feature,
no new milestone-1 requirement, no widened acceptance criterion.

## N11 — minor, non-blocking: tie "enabled audio track" to ADR-003's mix state explicitly

*Section: Audio-master and video policy.*

"…when the session has an enabled audio track with non-zero output volume…"
is testable and grounded, but "enabled" does not name which state it refers
to. The codebase has two audio-adjacent toggles that must not be confused:
`TimelineAudioMixState.isVideoTrackMuted`, which is mix-affecting and
correctly belongs in this determination, and `TimelineViewState
.isAudioTrackVisible`, which ADR-003 already classifies as view-only and
explicitly *not* mix-affecting. "Enabled" reads as almost certainly meaning
the former, but the word alone does not rule out the latter reading.

**Suggested edit, take-or-leave.** Replace "an enabled audio track" with "a
resolved audio layer not silenced by the snapshot's mix state (ADR-003's
`TimelineAudioMixState`)," or add a one-clause parenthetical ruling out the
view-only track-visibility toggle. This does not need to gate acceptance —
the intended meaning is not genuinely in doubt, only its precision against a
future reader who has not internalized ADR-003's own view/mix distinction.

## Is ADR-004 implementation-ready?

**Yes.** Every finding from review 1 is closed, no new blocking issue was
found, and the one remaining item (N11) is optional polish rather than a
condition for acceptance.

## Status change

**ADR-004 may change from Proposed to Accepted as written.** N11 may be
applied in the same pass or left for a future editorial sweep; neither
choice blocks the status change.

## Reviewer's note

This round found nothing that required going back to source code or
cross-referencing anything beyond the four documents already in scope —
every fix map directly onto review 1's own recommended edit, which is the
sign of a clean resolution pass rather than one that reopened questions
along the way. The one thing worth carrying forward to ADR-005: this
document's "hands off observations through a lock-free or otherwise
non-blocking path" sentence is the first explicit real-time-safety
statement in the series for the audio callback thread specifically: ADR-005
should treat it as a constraint already accepted, not a detail still open
for debate.
