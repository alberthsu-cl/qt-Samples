# ADR-007 Review 3 — Final Acceptance Gate

Verdict: **Accepted**

Round: 3 (final acceptance gate)

Revision reviewed: `2775e9d` *Resolve ADR-007 command type collision*
(diff confined to ADR-007, +12/-0 lines).

Reviewed against
[ADR-007](../decisions/0007-framework-neutral-core-and-ui-adapters.md) as it
stands after `2775e9d`, [review 1](adr-007-review-1-framework-boundary.md),
[review 2](adr-007-review-2-resolutions.md), ADR-001 through ADR-006 (all
six Accepted, closed),
[`../target-playback-architecture.md`](../target-playback-architecture.md),
and the current source: `src/ProjectState.h`, `src/EditorCommandController.h`,
`src/EditorCommandController.cpp`, and every other file in `src/` and
`tests/` that references `PlaybackCommand`.

## Verdict

Both findings from review 2 are resolved. B2's fix is the exact rename
recommended — `PlaybackCommand` to `LegacyPlaybackCommand` — and this round
verified its real scope against the actual codebase rather than the ADR's
own estimate: the type is referenced in thirteen files, not the handful a
first reading of `EditorCommandController` alone would suggest, and every
one of them is a mechanical rename with no hidden semantic dependency on the
name. N5 resolved with more precision than requested, correctly
distinguishing which `PlaybackCommand` members are genuine UI-gesture
intents from which are lifecycle events that bypass `EditorIntent`
entirely — a real architectural clarification, not just an acknowledgment
that something was missing.

One very small wording-order note remains, worth a future pass but not a
gate.

## B2 — resolved, and the rename's real scope was independently verified

*Section: Shared command and publication boundary.*

> The current four-member `PlaybackCommand` enum in `ProjectState.h` is
> renamed `LegacyPlaybackCommand` during coexistence. The eight-alternative
> engine `PlaybackCommand` is the only command type at the new engine
> boundary. The temporary `IPlaybackBackend` adapter translates legacy
> controls to the new command route and is removed once the core-backed
> path becomes the default.

This is exactly the fix review 2 recommended, including the exact
suggested name. Three things were checked directly against the source
rather than taken on the ADR's word:

**The rename's actual scope is wider than either prior review's text
implied, and that is fine.** A repository-wide search for `PlaybackCommand`
finds it in thirteen files: `EditorCommandController.cpp`, `EditorSession.h`
/`.cpp`, `MainFrame.h`/`.cpp`, `PlaybackBackend.h`/`.cpp`,
`QtMediaPlaybackBackend.h`/`.cpp`, `QtTransportHost.h`/`.cpp`,
`QtTransportPanel.cpp`, and `ProjectState.h` itself — reaching as far as the
Qt transport panel's own button-click handlers
(`emit playbackCommandRequested(static_cast<int>(PlaybackCommand::Stop))`)
and roughly a dozen call sites in `tests/MiniEditorCoreTests.cpp`. Every
occurrence is a plain type-name reference with no serialization, string
matching, or reflection dependent on the name `PlaybackCommand` specifically
— confirmed by inspection, not assumed. A global rename is mechanical and
low-risk despite touching more files than a first read would suggest; it
requires no additional design decision, which is the property the ADR's
one-paragraph fix depends on being true.

**The new name introduces no second collision.** `LegacyPlaybackCommand`
does not already appear anywhere in `src/`.

**The retirement plan matches an already-Accepted decision rather than
inventing one.** "The temporary `IPlaybackBackend` adapter translates
legacy controls to the new command route and is removed once the
core-backed path becomes the default" is consistent with the target
document's own framing — "The current `IPlaybackBackend` can serve as the
temporary integration seam. Its implementation will delegate commands to
the new engine rather than growing more reconciliation logic" — and with
ADR-002's migration step "Remove or narrow legacy playback mutation APIs
after source preview also migrates." This closes the loop cleanly: the
renamed legacy enum's lifetime is bounded by the same event that already
retires the façade that owns it.

**Conclusion: the migration path is feasible and non-conflicting**, which
is what this round was specifically asked to verify.

## N5 — resolved, with a genuine architectural clarification beyond what was asked

*Section: Shared command and publication boundary.*

> `EditorIntent` represents user-facing editor actions. It must gain
> explicit seek and rate intents as those controls migrate. `OpenSource`,
> `InstallSnapshot`, and `Shutdown` are project/engine lifecycle commands
> sent by their owning adapter or service, not synthetic UI intents; toggle
> playback may resolve to `Play` or `Pause` from the last published phase.

Review 2's N5 asked only that the ADR acknowledge `EditorIntent` does not
yet cover the full `PlaybackCommand` surface. The resolution does more than
acknowledge a gap — it explains *why* three of the eight `PlaybackCommand`
members (`OpenSource`, `InstallSnapshot`, `Shutdown`) were never expected to
have an `EditorIntent` counterpart at all: they are not discrete user
gestures with several possible outcomes the way `Play`/`Pause`/`Seek` are —
they are lifecycle events raised by whichever adapter or service already
owns them (selecting a media asset, committing an edit, application
shutdown). This is a correct and useful distinction that resolves a
question the original ADR text left implicit, not merely a checklist item
closed. The remaining five members (`Play`, `Pause`, `Stop`, `Seek`,
`SetRate`) are the ones `EditorIntent` genuinely needs to grow toward, and
the ADR says so plainly for `Seek`/`SetRate` while correctly noting
`TogglePlayback`'s existing many-to-one resolution against last-known phase.

## Non-blocking note

**N6 — the general rule and its exceptions are stated several paragraphs
apart.** "Adapters first reduce framework-specific gestures to the shared
framework-neutral `EditorIntent`; shared translation maps that intent to
the appropriate `PlaybackCommand`" reads, on its own, as a universal rule.
The exception for `OpenSource`/`InstallSnapshot`/`Shutdown` arrives six
lines later, in the paragraph resolving N5. A reader who stops at the first
sentence would reasonably but incorrectly conclude every `PlaybackCommand`
value passes through `EditorIntent`. Moving the exception adjacent to the
rule, or adding a forward reference from one to the other, would remove the
gap between them. This is a paragraph-ordering suggestion, not a
correctness issue — both statements are compatible once both are read.

## Is ADR-007 implementation-ready?

**Yes.** Both findings from review 2 are resolved, the rename's feasibility
was checked against the real codebase rather than inferred from the ADR's
own text, and no new blocking issue was found. N6 is optional and does not
gate acceptance.

## Status change

**ADR-007 may change from Proposed to Accepted as written.**

## Reviewer's note

This closes the seventh and final ADR on the target document's original
roadmap. Across three rounds, this ADR's own review found the sharpest
concrete defect in the series — a symbol collision that would have failed
to compile — precisely because the round's own instructions asked the
question this series has learned to ask of every prior ADR only sometimes:
not just "does the text agree with itself and its neighbors," but "does the
plan survive contact with the code it has to run inside." Both checks
matter; this round is a reminder that the second one is not optional when a
decision touches code that already exists.
