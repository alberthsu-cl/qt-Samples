# ADR-007 Review 1 — Framework-Neutral Core and Qt/MFC UI Adapters

Verdict: **Accept with revisions**

Round: 1 (original review)

Primary document:
[`../decisions/0007-framework-neutral-core-and-ui-adapters.md`](../decisions/0007-framework-neutral-core-and-ui-adapters.md)
(Status: Proposed)

Reviewed against ADR-001 through ADR-006 (all six Accepted, closed),
[`../target-playback-architecture.md`](../target-playback-architecture.md),
and the current build (`CMakeLists.txt`'s `MINI_EDITOR_USE_QT` option).

## Verdict

This is the most citation-disciplined document in the series so far. Every
quoted contract — the five core interfaces, the eight-member
`PlaybackCommand` variant, the five-member `PlaybackEvent` variant including
`PlaybackCommandRejected` — was checked member-for-member against its source
(ADR-002, ADR-005, and the target document's Decision 10) and matches
exactly, including picking up the post-acceptance `PlaybackCommandRejected`
addition rather than a stale pre-fix copy. The prohibited-type list
(`QObject*`, `QVideoFrame`, `QImage`, `HWND`, `CDC`, `CString`) is more
concrete than any equivalent list earlier in the series, and the flag-safety
rule — "At no point may a flag create two authorities for one preview
session" — is exactly ADR-002/ADR-005's one-authority thesis carried forward
to the UI-migration mechanism specifically, not restated generically.

One issue blocks acceptance, and it is a genuine build-architecture
mismatch rather than a documentation gap: acceptance criterion 3 requires
comparing MFC and Qt adapter command output directly, but the current
project compiles MFC-adapter and Qt-adapter code as mutually exclusive
alternatives behind one compile-time flag, never into the same binary. As
written, no single test run can exercise the comparison the criterion
demands.

Four non-blocking items would tighten wording without changing any
decision.

## Findings

| ID | Finding | Classification |
| --- | --- | --- |
| B1 | Criterion 3 is not testable under the current flag-exclusive build structure | **Blocking** |
| N1 | Qt adapter's two delivery mechanisms aren't scoped to which deployment each applies to | Non-blocking |
| N2 | The 5-step migration sequence doesn't cross-reference the per-ADR sequences it integrates | Non-blocking |
| N3 | Criterion 8 could name the existing test suites it means, matching the concrete form other ADRs settled into | Non-blocking |
| N4 | "a thread-safe UI notification queue or event-sink port" reads as two mechanisms when it names one | Non-blocking |
| — | Core interfaces, prohibited types, command/event variants | Verified member-for-member against ADR-002/005 and the target document |
| — | MFC notification bridge, `processEvents()` retirement | Consistent with ADR-005's already-Accepted text |
| — | Presentation-adapter side-effect prohibition | Correctly extends ADR-003 with a new, non-contradictory invariant |
| — | Shutdown, decoder backend, GPU API, codec choice | Correctly left to ADR-005 / deferred |

## Framework-neutral core boundaries and prohibited types

Correct and unusually concrete. The five interfaces
(`IPlaybackClock, IVideoDecodeService, IAudioDecodeService, IVideoCompositor,
IPlaybackEventSink`) match the target document's Decision 10 exactly, in the
same order. The prohibited-type list names the specific handles most likely
to leak (`QObject*`, `QVideoFrame`, `QImage`, `HWND`, `CDC`, `CString`)
rather than only the generic "no Qt/MFC objects" phrasing earlier ADRs used
— a real improvement in checkability, since a header can be grepped for
these names directly. "Framework-specific media handles may exist inside a
concrete decoder or compositor adapter, but are converted to a
framework-neutral buffer/descriptor before crossing into the core" correctly
matches ADR-003's `VideoFrameBuffer` abstraction and ADR-005's adapter
boundary.

## Command/event compatibility with ADR-002 and ADR-003

Verified exactly, not assumed. `PlaybackCommand`'s eight members
(`OpenSource, InstallSnapshot, Play, Pause, Stop, Seek, SetRate, Shutdown`)
match ADR-002's closed variant precisely — notably, this document does not
repeat ADR-005's original "clock selection" ninth-member error, which that
ADR's own review round had to correct. `PlaybackEvent`'s five members match
the target document's current text, including `PlaybackCommandRejected`,
confirming this ADR was written against the already-corrected series state
rather than an earlier snapshot.

"Adapters never modify an event to make it current; they pass the immutable
value to the core-side consumer that validates identity" is the right
restatement of ADR-003's consumer-decides-currency rule, correctly assigning
responsibility to the same layer ADR-005's B1 fix settled on.

## Qt/MFC ownership, event-loop, and notification-bridge behavior

Correct, and consistent with material this series worked to establish
across two prior ADRs rather than reintroducing it independently. The MFC
adapter section — "posts one Windows message to `MainFrame` when the UI
notification queue becomes non-empty; the GUI thread drains that queue and
updates MFC controls and any embedded Qt widgets" — matches the target
document's "UI notification bridge" mechanism (Decision 7) almost verbatim,
and the explicit prohibition on calling `QCoreApplication::processEvents()`
matches ADR-005's own retirement of that legacy pattern, word for word in
spirit.

The Qt adapter section is looser. See N1.

## Preview presentation adapter responsibilities

A genuine, non-contradictory addition rather than a restatement. ADR-003
established atomic frame publication and identity-gated acceptance;
ADR-007 adds the invariant ADR-003 did not need to state because it wasn't
yet describing a second framework's adapter: "They do not seek, play,
pause, change the active sequence, or mutate editor selection as a side
effect of painting." This closes a real gap — nothing upstream explicitly
forbade a presentation adapter from having side effects, and a naive MFC or
Qt paint handler is exactly the kind of code that accumulates them.

## Feature-flag and coexistence safety

Correct and appropriately terse, because the hard work was already done.
"A flag may route timeline preview to the new core while legacy source
preview remains on the existing backend, as ADR-002 permits" cites a real,
already-Accepted staged-migration allowance rather than inventing new
latitude. "At no point may a flag create two authorities for one preview
session" is ADR-005's B8 fix (mutually exclusive flag ownership) generalized
from "legacy versus new playback path" to "which UI framework issued the
command" — the same rule, correctly recognized as covering both cases
without needing to be re-argued.

## Acceptance-criteria testability

Seven of eight criteria are objective and testable under the patterns this
series has already established. Criterion 1 matches the codebase's existing
zero-Qt-linked test target precedent directly. Criterion 3 is where this
round's blocking finding lives.

### B1 — Criterion 3 cannot be exercised by a single test run under the current build

*Section: Acceptance criteria, item 3; also Qt adapter, MFC adapter.*

> 3. MFC and Qt adapters issue byte-for-byte equivalent command values for
>    the same user intent.

This is a well-formed, meaningfully precise criterion — `PlaybackCommand`
members are plain value types, so "byte-for-byte equivalent" is a real,
checkable property, not a vague aspiration. The problem is not the
criterion's design; it is that the project's actual build makes it
unreachable as stated.

`CMakeLists.txt`'s `MINI_EDITOR_USE_QT` option selects, at configure time,
which entire set of panel source files compiles: the Qt panels and their
adapters when `ON`, the MFC panes when `OFF`. The two adapter
implementations for a given panel are never compiled into the same binary,
which the project's own existing `vs2022-x64` / `vs2022-mfc-x64` build-tree
split (and this series' own recurring practice of testing both
configurations separately) already confirms in practice. A test that
constructs both an MFC adapter and a Qt adapter for "the same user intent"
and compares their output side by side cannot exist as a single test target
under this structure — one of the two adapters is simply not compiled into
whichever binary the test runs in.

This is squarely a testability gap for an explicit acceptance criterion, not
a hypothetical concern; the review was asked to check exactly this.

**Minimal fix — two options, either is acceptable, neither reopens a
decision:**

- Narrow the claim to what is actually built: extract the *command-
  translation* step specifically (mapping one user gesture — a menu ID, a
  Qt button click — to one `PlaybackCommand` value) into small,
  framework-independent-input functions that can be linked and compared
  in one core test binary, independent of which full panel implementation
  the flag selects. This is a small refactor, not a new architectural
  decision — the translation logic already has to exist somewhere in each
  adapter.
- Or explicitly downgrade the criterion to cross-configuration parity
  review: state that criterion 3 is verified by building both
  configurations and comparing documented command mappings, not by a
  single automated test — matching the "inspection, not test" downgrade
  this series has used before for similarly-shaped criteria (for example,
  ADR-001's original criterion 1).

Criterion 8 does not have this problem and needs no change: "Qt and MFC
panel replacement tests preserve existing timeline playback…" is naturally
read as cross-build regression consistency — run the existing suites in
both configurations and confirm consistent behavior — which is exactly how
this project's tests already run today, not a same-run comparison.

## Contradictions, missing invariants, or scope expansion

No contradiction with any Accepted ADR. No scope expansion: every addition
in this document narrows an already-open question (which types are
prohibited, which invariant a presentation adapter must not violate, how a
flag may not be misused) rather than introducing new milestone-1
requirements. The one missing invariant is B1's testability gap; everything
else checked — thread ownership, shutdown, decoder/GPU deferrals — is
correctly left where ADR-005 and ADR-003 already placed it.

## Non-blocking improvements

**N1 — Scope the Qt adapter's two delivery mechanisms.** "Qt-facing results
are delivered to the Qt GUI thread through queued signals or a GUI-thread
drain of the shared notification queue" offers two valid mechanisms without
saying which applies where. The likely intent: the current MFC-hosts-Qt
coexistence configuration uses the MFC adapter's own message-driven drain
(which already "updates MFC controls **and any embedded Qt widgets**" per
that section), while native Qt queued signals become relevant only for a
future Qt-only shell with its own event loop. One clause naming this split
would remove the ambiguity.

**N2 — Cross-reference the per-ADR migration sequences this one
integrates.** ADR-002, ADR-003, ADR-004, and ADR-005 each already specify
their own migration-strategy steps at a finer grain than this document's
five. The two are not in tension — ADR-007's sequence is a coarser,
higher-level view — but nothing says so explicitly, and a reader could
otherwise wonder whether this is a competing plan.

**N3 — Name the existing test suites in criterion 8.** "Qt and MFC panel
replacement tests" would be more concrete as "the existing
`MiniEditorCoreTests` and Qt widget test suites," matching the pattern other
ADRs' analogous criteria settled into after their own review rounds (for
example, ADR-002's criterion 9 and ADR-004's criterion 9, both reworded
during resolution to name a concrete check rather than a general claim).

**N4 — Tighten "queue or event-sink port."** "The core exposes a
thread-safe UI notification queue or event-sink port" reads as offering two
alternative mechanisms, when the intent is very likely that
`IPlaybackEventSink` — already named earlier in the same document — *is*
the port through which the queue is accessed. "…a thread-safe UI
notification queue, accessed through the `IPlaybackEventSink` port" would
remove the apparent duplication.

## Reviewer's note

This document reads as though every prior ADR's review history was consulted
before it was written, not after: it avoids ADR-005's command-count mistake,
carries forward ADR-005's `processEvents()` retirement and ADR-003's
identity-consumer rule without drift, and generalizes ADR-005's
flag-authority rule rather than re-deriving it. The one place it needed a
fresh check rather than a citation is exactly where citation could not help
— whether an acceptance criterion is actually exercisable against the
project's real build graph, which required reading `CMakeLists.txt`, not
another ADR.
