# ADR-005 Review 2 — Resolution Verification

Verdict: **Accepted**

Round: 2 (resolution review, doubling as an acceptance check)

Revision reviewed: `e0c893b` *Revise ADR-005 after thread ownership review*
(diff confined to ADR-005).

Reviewed against
[ADR-005](../decisions/0005-engine-decoder-and-ui-thread-ownership.md) as it
stands after `e0c893b`, [review 1](adr-005-review-1-thread-ownership.md),
ADR-001 through ADR-004 (all four Accepted, closed), and
[`../target-playback-architecture.md`](../target-playback-architecture.md).

## Verdict

All eight findings from review 1 — B1 through B4 and N1 through N4 — are
resolved, four of them (B1, B3, B4, N1, N2, N4) with wording that matches the
recommended minimal fix almost verbatim, and B2 resolved by actually
completing the mechanism rather than re-deferring it again. Every added and
removed line in the revision was checked individually against the eight
findings; nothing outside their scope was altered.

No new blocking finding. Two small, genuinely optional items are worth
recording for a future editorial pass: exactly which step performs "closing
command acceptance" during shutdown is still one inference away from
explicit (N5), and the new decode-failure-propagation text has no
corresponding acceptance criterion of its own (N6). Neither gates
acceptance.

## Resolution status

| ID | Resolution applied | Verified against text |
| --- | --- | --- |
| B1 | Criterion 4 reworded to match the body | "Consumers reject stale decoder/compositor results using ADR-003 identities; workers never decide their own currency." Matches the recommended fix almost word for word, and now agrees with the body's own "identity validation at the consumer is the authority." **Resolved.** |
| B2 | Failure-propagation path stated explicitly | "A decode failure or an unavailable-media descriptor is delivered as an immutable failure result; the engine consumer validates its identity and performs the `PlaybackPhase::Failed` transition with a framework-neutral `PlaybackError`. The detailed decoder error taxonomy remains an implementation contract owned by this ADR…" This redeems ADR-003's promise rather than deferring it a third time: the architectural question (which layer transitions to `Failed`, what type carries it, no Qt leakage) is answered; only the implementation-level taxonomy is left open, on the same terms `PlaybackError` was already left opaque under ADR-002. **Resolved.** |
| B3 | Coalescing scoped to scheduled work only | "…may have its scheduled decode/composition work coalesced, but the command itself is still individually accepted or rejected and acknowledged per ADR-002." Removes the third, silently-dropped fate the ambiguity risked. **Resolved.** |
| B4 | "Clock selection" removed from the command list | "Commands include play, pause, stop, seek, rate change, source replacement, snapshot installation, and shutdown; clock selection is an engine-internal decision governed by ADR-004, not a new caller-facing command." Exactly eight items, matching ADR-002's closed `PlaybackCommand` variant, with an explicit disambiguating clause. **Resolved.** |
| N1 | Shutdown acknowledgment named as its own step | New step 4: "Engine publishes the final ADR-002 shutdown acknowledgment status, with the playback phase unchanged, before command acceptance closes." Subsequent steps renumbered correctly (now six steps total). **Resolved.** |
| N2 | UI notification bridge named; `processEvents()` retirement stated | "…receives immutable publications through the MFC message loop through the target architecture's UI notification bridge… The legacy pattern of calling `QCoreApplication::processEvents()` from an MFC timer is retired by this model." **Resolved**, with a small grammar note — see below. |
| N3 | `EditorSession` named in the ownership table | The GUI/UI thread row now reads "`EditorSession`, Qt widgets, MFC windows, view state, editor commands," matching the target document's equivalent table. **Resolved.** |
| N4 | Forward note on a future decoder/compositor split | "This worker domain may later split into dedicated decoder and GPU-compositor domains when D3D11/QRhi composition is introduced; the ownership and result identity rules remain unchanged." **Resolved.** |

## New checks for this round

**Contradictions.** None found. The B2 fix does not contradict the four-domain
ownership table: it correctly stays in future tense ("may later split") and
does not prematurely split the table's still-single "Decoder/compositor
workers" row. The B4 fix does not create a new mismatch elsewhere — "clock
selection" as *owned state* (engine thread) versus *not a command* are now
consistent with each other and with ADR-004, where before they were not.

**Missing ownership rules.** None found beyond the two items already closed
by B1 and B2. The failure-propagation addition correctly keeps ownership
where it already was: the worker produces an immutable result (now
including a failure variant), the engine consumer decides currency and
performs the transition — no new object crosses a domain boundary that
was not already crossing one.

**Unclear shutdown behavior.** Mostly closed by N1. One small residual
ambiguity: step 4 states the acknowledgment is published "before command
acceptance closes," but no step is explicitly labeled as the moment
acceptance *does* close — it is implied to happen immediately after step 4,
between it and step 5's event-loop draining, but a reader has to infer that
placement rather than read it. See N5.

**Untestable acceptance criteria.** None of the ten are untestable, but the
criteria list was not extended to cover the new failure-propagation text
added to close B2. Criterion 4 covers *stale* results; the decode-failure
path is a different scenario (a well-formed, current result that reports
failure) and has no criterion of its own. See N6.

**Scope expansion.** None. Every change in the diff narrows or completes an
already-open question; nothing widens what milestone 1 must build. The
GPU-compositor forward note (N4) is explicitly deferred, not newly required.

## N5 — non-blocking: name the exact moment command acceptance closes

*Section: deleteLater() and shutdown ordering.*

The six-step sequence is otherwise complete, but "before command acceptance
closes" in step 4 has no corresponding step that performs the closing. The
likely intended reading — acceptance closes immediately after the
acknowledgment is published, before event-loop draining begins in step 5 —
is reasonable but requires the reader to infer placement rather than see it
stated. A one-clause addition to step 4 ("…before command acceptance closes,
which the engine does immediately afterward") or a new short step 5 would
remove the inference.

## N6 — non-blocking: add an acceptance criterion for the new failure-propagation text

*Section: Acceptance criteria.*

The body now states a concrete, testable behavior that was not there in
review 1: a decode failure or unavailable-media descriptor becomes a
`PlaybackPhase::Failed` transition carrying a `PlaybackError`, once identity
is validated at the consumer. No criterion currently exercises this path
independently of criterion 4's stale-result rule, which is a different
scenario. Suggested addition: "A validated decode failure or
unavailable-media result transitions the session to `PlaybackPhase::Failed`
with a `PlaybackError`; a stale one does not."

## Minor editorial note (not a tracked finding)

The MFC coexistence sentence reads "…receives immutable publications through
the MFC message loop through the target architecture's UI notification
bridge…" — the repeated "through … through" is a small grammatical
artifact of the edit, not a substantive issue. Worth a pass in a future
editorial sweep; does not affect meaning or testability.

## Is ADR-005 implementation-ready?

**Yes.** Every finding from review 1 is closed, no new blocking issue was
found, and N5/N6 are optional refinements rather than conditions for
acceptance — the same shape review 2 of ADR-004 reached with its own single
non-blocking residual item.

## Status change

**ADR-005 may change from Proposed to Accepted as written.** N5 and N6 may
be applied in the same pass or left for later; neither blocks the status
change.

## Reviewer's note

This is the fourth resolution round in the series (after ADR-001, ADR-002,
and ADR-004) to close every finding from its preceding round cleanly on the
first attempt, and the second (after ADR-004) to do so with zero new
blocking issues. The pattern holds across all five ADRs now reviewed:
findings that name a concrete quote and a one-sentence fix resolve cleanly;
the only round that needed a second blocking pass (ADR-002) was the one
where the original finding described a structural type problem rather than
a missing sentence. ADR-005's four blocking findings were all of the
missing-sentence kind, and all four came back as exactly that.
