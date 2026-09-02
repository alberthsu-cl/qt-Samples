# ADR-005 Review 1 — Engine, Decoder, and UI Thread Ownership

Verdict: **Accept with revisions**

Round: 1 (original review)

Primary document:
[`../decisions/0005-engine-decoder-and-ui-thread-ownership.md`](../decisions/0005-engine-decoder-and-ui-thread-ownership.md)
(Status: Proposed)

Reviewed against ADR-001 through ADR-004 (all four Accepted, closed) and
[`../target-playback-architecture.md`](../target-playback-architecture.md).

## Verdict

The four-domain ownership table is clean, exhaustive, and correctly
consolidates the target document's five-row thread table into four without
losing any constraint — video and audio workers merge into one
"Decoder/compositor workers" domain because ADR-005's model treats them
symmetrically, which is a genuine simplification, not a loss of precision.
The Qt affinity rules match patterns already used in this codebase (queued
connections with registered value types), the shutdown ordering is a
plausible five-step sequence, and the audio-callback real-time constraints
are a correct, slightly stricter superset of ADR-004's own list.

Four issues block acceptance, and none require a new design decision — three
are corrections to make the document internally consistent or consistent
with an already-Accepted ADR, and one is redeeming a promise a closed ADR
already made by name to this document.

## Findings

| ID | Finding | Classification |
| --- | --- | --- |
| B1 | Acceptance criterion 4 contradicts the body text on who rejects stale results | **Blocking** |
| B2 | ADR-003's decoder-contract promise, made to this ADR by name, is unaddressed | **Blocking** |
| B3 | "May be coalesced" is ambiguous against ADR-002's per-seek acceptance model | **Blocking** |
| B4 | "Clock selection" is listed as a command, but ADR-002's closed command set has no such member and ADR-004 describes it as automatic | **Blocking** |
| N1 | Shutdown ordering does not name ADR-002's required final acknowledgment status as a step | Non-blocking |
| N2 | The MFC coexistence section is vaguer than the target document's own already-specified UI bridge mechanism | Non-blocking |
| N3 | `EditorSession` is not named in the GUI-thread ownership row, unlike the target document's equivalent table | Non-blocking |
| N4 | "Decoder/compositor workers" may need to split once a GPU compositor arrives | Non-blocking |
| — | Four-domain table, Qt affinity rules, audio real-time constraints | Resolved by / consistent with ADR-001 through ADR-004 |
| — | Decoder backend, lock-free queue library, audio API, GPU compositor choice | Correctly left as implementation detail |

## B1 — Acceptance criterion 4 contradicts the body on who rejects a stale result

*Sections: Decoder and compositor workers; Acceptance criteria.*

The body is explicit and correct:

> A worker receives an immutable request … It may finish after cancellation;
> **the consumer** rejects the result if session, generation, revision,
> media, or request identity is stale.
>
> Worker completion is delivered as an immutable result to the engine or
> presentation coordinator through a queue. **The completion callback itself
> does not decide whether a result is current; identity validation at the
> consumer is the authority.**

Acceptance criterion 4 says the opposite:

> 4. **Decoder/compositor workers reject stale results** using ADR-003
>    identities.

This is not a wording nuance — it names the wrong layer as the authority for
exactly the rule ADR-003 was written to establish (a producer never decides
its own currency; a consumer does). A test written against criterion 4 as
stated would exercise validation logic *inside* the worker, reintroducing the
distributed validation ADR-003 explicitly rejected in its own "Alternatives
considered" section ("Cancel every worker synchronously on seek or edit …
would make UI responsiveness part of the correctness contract").

**Minimal fix.** Reword criterion 4 to match the body: "The consumer rejects
stale worker results using ADR-003 identities; workers never decide their own
currency." No design change — the body already says this correctly.

## B2 — ADR-003's decoder-contract promise is unaddressed

*Whole document.*

ADR-003, after its own review series, added this sentence to close a finding
about unavailable media:

> Attempting to resolve it produces the deterministic media-failure/
> placeholder policy owned by the media adapter **and decoder contract
> defined by ADR-005**.

ADR-005 never mentions `MediaAvailability`, an unavailable descriptor, a
placeholder policy, or a decoder error taxonomy at all — the terms do not
appear anywhere in the document. Nor does ADR-005 describe *any* path for a
decode failure to become a `PlaybackPhase::Failed` transition with a
`PlaybackError`; the word "error" appears exactly once, inside a generic list
("frames, statuses, errors, and commands are immutable values"), with no
supporting mechanism.

This is a real gap, not a stylistic one: ADR-002 assigned decoder error
taxonomy to "ADR-003 and its decoder contract"; ADR-003 forwarded that
specific promise to ADR-005 by name; ADR-005 is the last ADR currently on the
roadmap and does not redeem it or forward it again. Contrast this with how
ADR-004 handled its own analogous deferral — it named "audio-buffer-underflow
recovery behavior" explicitly and added it to a "Deferred decisions" list
naming a plausible destination ("a follow-up implementation ADR"). ADR-005
has the same section and could do the same thing; it currently does neither.

**Minimal fix.** Add one paragraph, most naturally in "Decoder and compositor
workers": state how a decode failure or an unavailable-descriptor result
becomes a `PlaybackPhase::Failed` transition with a `PlaybackError` (the
mechanism is already implied — "Worker completion is delivered as an
immutable result to the engine … through a queue" already covers a failure
result, it just is not named as one). If the exact error taxonomy is still
undecided, say so explicitly and add it to "Deferred decisions" with a named
destination, exactly as ADR-004 did for its own open item.

## B3 — "May be coalesced" is ambiguous against ADR-002's per-seek model

*Section: Engine thread and command queue.*

> A command that is superseded by a newer seek or snapshot may be coalesced,
> but command ordering must remain observable at the engine boundary.

ADR-002's own Seek row states: "Every accepted seek immediately increments
generation, including repeated scrubbing seeks." That model has every seek
individually **accepted** — each gets processed and, if superseded before
completion, invalidated by the next generation. It has no third fate between
"accepted" (leading to a `lastAppliedCommandId` acknowledgment) and
"rejected" (leading to a `PlaybackCommandRejected`).

"Coalesced" is not defined precisely enough to tell whether it means the same
thing in different words — "accept the command, then let the next generation
immediately supersede its scheduled work" — or something ADR-002 does not
have a category for: dropping a queued command before it is ever accepted or
acknowledged. If it is the latter, a caller's `PlaybackCommandId` would never
receive either acknowledgment or rejection, which is a real hole in the
command contract ADR-002 established.

**Minimal fix.** One sentence: "Coalescing applies to the scheduled decode or
composition work a command would have triggered, never to the command's own
acceptance and acknowledgment; every submitted command is still individually
accepted (or rejected) and acknowledged per ADR-002, including one superseded
before its work completes." This does not change any behavior already
decided — it removes a reading that would.

## B4 — "Clock selection" as a command exceeds ADR-002's closed command set

*Section: Engine thread and command queue; ownership table.*

> Commands include play, pause, stop, seek, rate change, source replacement,
> snapshot installation, **clock selection**, and shutdown.

Counting: this is nine items. ADR-002's `PlaybackCommand` variant, which is
Accepted and closed, has exactly eight:
`OpenSource, InstallSnapshot, Play, Pause, Stop, Seek, SetRate, Shutdown`.
"Clock selection" is not one of them.

The term also conflicts with how ADR-004 itself describes the concept.
ADR-004 frames clock selection as an *automatic* consequence of observed
state, never a caller-issued command: "The master-clock policy is: when
audible audio is active, the audio-output clock is authoritative; otherwise,
a monotonic clock is authoritative … Enabling or disabling audible audio is a
clock selection boundary and therefore re-anchors." ADR-005's own ownership
table agrees with this framing elsewhere — it lists "clock selection" as part
of what the engine thread *owns* (state), consistent with ADR-004 — which
makes listing it a second time as something a *command* produces
self-inconsistent within ADR-005 as well as inconsistent with ADR-002.

A more charitable reading is that this is a terminology drift from ADR-004's
separate concept "clock replacement" — swapping which `IPlaybackClock`
*implementation* is installed (`steady_clock` today, an audio-device clock
later), which is a rare, administrative event rather than a per-playback
runtime action. Even under that reading, "clock replacement" is still not a
member of ADR-002's closed command set, so the same question applies: is this
a new caller-facing command needing a formal amendment to ADR-002 (which this
document cannot perform unilaterally, since ADR-002 is closed), or is it
purely an engine-internal event with no external command at all?

**Minimal fix.** Remove "clock selection" from the list of commands. If a
distinct administrative action to swap the clock implementation is genuinely
needed, name it explicitly as a new command requiring a formal amendment to
ADR-002's `PlaybackCommand` variant, rather than introducing it silently
inside ADR-005's prose.

## Ownership of mutable state by thread

Correct and complete otherwise. The rule "No object is owned by two domains.
A thread may read an immutable value published by another domain, but it
never reads that domain's mutable state" is the right generalization of
ADR-002's engine-thread-authority rule to workers and the audio callback, and
the four-domain table is exhaustive against every runtime object this series
has introduced (`PlaybackSession`, snapshots, decoder/compositor instances,
the audio ring buffer, Qt widgets, MFC windows).

## Command and result queue boundaries

Sound apart from B3. "The queue never executes user callbacks inline on a
producer thread" is the correct anti-reentrancy rule, and "command ordering
must remain observable at the engine boundary" is the right property to
require even before coalescing is clarified.

## Qt QObject affinity and queued delivery

Correct, and consistent with an existing pattern in this codebase, not just
an abstract rule: cross-thread delivery via queued connections with
registered value types (`Q_ARG` plus `qRegisterMetaType`) is already how the
project's own worker-thread effect pipeline hands frames back to the UI
thread. ADR-005 is formalizing a pattern the code already uses correctly,
not inventing an untested one.

## deleteLater() and shutdown ordering

The five-step sequence is sound and each step follows from the one before
it. "No worker completion is allowed to call a deleted receiver" backed by
"a lifetime-safe handle whose consumer still performs identity validation"
is the right defense-in-depth: even if a disconnect race were to slip
through, ADR-003's identity check is still there to discard the result.

One step is implicit rather than stated. ADR-002's `Shutdown` row requires
"one final status whose playback phase is unchanged" to be published before
command acceptance closes. The five-step list does not name this explicitly
— it is presumably covered by step 4's "queued completions … process[ed],"
but a reader has to infer that. See N1.

## Audio callback real-time safety

Correct, and a strict superset of ADR-004's own list (adds "perform file or
decoder I/O" and "call into Qt widgets" to ADR-004's block/allocate/lock-
contend/mutate-authority set). No contradiction; a good elaboration.

## Stale-result and generation interactions

The design is right — "identity validation at the consumer is the authority"
is exactly ADR-003's rule, correctly generalized to the worker/engine
boundary. The one place this section actually causes trouble is where the
acceptance criteria misstate it. See B1.

## MFC/Qt adapter coexistence

The contract itself — both adapters "issue the same engine commands and
consume the same immutable publications" without either becoming "a second
playback authority" — is correct and is the right generalization of ADR-002's
adapter-neutrality rule. It is less specific than it could be given the
target document already solved the concrete mechanism. See N2.

## Acceptance-criteria testability

Nine of ten criteria are objective and testable under MSVC C++17 with fake
ports, hardware-free, matching the pattern this series has established
(criterion 10 explicitly requires exactly that). Criterion 4 is testable
only after B1's correction — as worded, it specifies a test against the
wrong component.

## Contradictions, missing invariants, or scope expansion

Three internal-or-cross-ADR contradictions (B1, B3, B4) and one missing
invariant that a closed ADR already assigned here by name (B2). No
unrelated scope expansion: every other addition in this document — the
domain table, the Qt affinity rules, the shutdown sequence, the real-time
audio constraints — narrows an already-open question rather than widening
what milestone 1 must build.

## Minimal exact edits

| # | Section | Edit | Ref |
| --- | --- | --- | --- |
| 1 | Acceptance criteria 4 | Reword to name the consumer, not the worker, as the authority for stale-result rejection. | **B1** |
| 2 | Decoder and compositor workers (or a new paragraph) | State the failure-propagation path from a decode/unavailable-descriptor result to `PlaybackPhase::Failed` and `PlaybackError`, or explicitly re-defer it by name with a destination, as ADR-004 did for its own open item. | **B2** |
| 3 | Engine thread and command queue | Clarify that coalescing applies to scheduled work only, never to a command's own acceptance and acknowledgment. | **B3** |
| 4 | Engine thread and command queue | Remove "clock selection" from the command list, or name it as a new command requiring a formal ADR-002 amendment. | **B4** |
| 5 | deleteLater() and shutdown ordering | Add publishing the final ADR-002 acknowledgment status as its own numbered step. | N1 |
| 6 | MFC coexistence boundary | Name the target document's "UI notification bridge" mechanism explicitly and state that reliance on `QCoreApplication::processEvents()` from the MFC timer is retired by this model. | N2 |
| 7 | GUI/UI thread ownership row | Name `EditorSession` explicitly, matching the target document's equivalent table. | N3 |
| 8 | Decoder and compositor workers | Add one sentence noting this domain may split once a GPU compositor (D3D11/QRhi) arrives, per the target document's own forward-looking note. | N4 |

## Reviewer's note

Three of the four blocking findings in this round are internal-consistency
defects rather than missing decisions — the document disagrees with itself
(B1), or introduces a term wide enough to be read as contradicting an
Accepted ADR (B3, B4) — which suggests this draft was written from the
model rather than checked against the closed contracts it depends on
line by line. B2 is the more consequential one: it is the second time in
this series a promise named a specific downstream ADR by number, and the
second time that ADR did not pick it up. ADR-004 modeled the right way to
leave a decision genuinely open — name it, and name where it goes next.
