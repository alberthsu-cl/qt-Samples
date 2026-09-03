# Milestone 4 — Media integration

**Goal:** give the engine real decode, audio, and composition work, running on
real threads, with a real MFC/Qt UI notification path — all behind a feature
flag that defaults off, so the current application's behavior is unchanged
until Milestone 5 deliberately switches it on for comparison.

This is the largest and riskiest milestone so far. Milestones 1-3 only ever
touched `MiniEditorPlaybackCore` and tested it with fake ports; nothing they
built has run on a second thread or talked to a real `QMediaPlayer`. A
reconnaissance pass before planning this milestone confirmed:

- **No concurrency primitive exists anywhere in this codebase yet** — no
  `std::thread`, `std::mutex`, `std::condition_variable`, or third-party
  queue. Milestone 4 introduces the application's first real background
  thread.
- **No MFC notification-bridge infrastructure exists** — `MainFrame` has no
  custom Windows message, no `PostMessage`, no queue-drain handler. ADR-007's
  "MFC host posts a Windows message to `MainFrame`" is aspirational target
  text, not something to extend.
- `TimelinePlaybackResolver`, `MediaPlaybackPlan`, and
  `TimelineAudioPlaybackPlan` still resolve from live `TimelineModel`/
  `EditorSession`/`MediaLibrary` state and have no dependency on
  `SequencePlaybackSnapshot` at all. ADR-003's migration step 3 ("Adapt
  `TimelinePlaybackResolver` to consume only a snapshot") is unstarted.
- The existing `QtMediaPlaybackBackend` is a real, working implementation,
  but it is exactly what ADR-002/ADR-005 retire: two independent
  `QMediaPlayer`/`QAudioOutput` pairs with no compositor, driven by the MFC
  timer, calling `QCoreApplication::processEvents()` from inside that timer
  tick.

Given that, this milestone stays in fake-port, core-only territory for as
long as possible (matching Milestones 1-3's proven, fully-testable pattern)
and only reaches into live Qt/MFC application code in its last two issues,
where automated tests alone cannot fully verify real media/UI behavior —
those are called out explicitly below rather than treated as equivalent in
risk to the rest.

## Dependency graph

```text
M4-01  PreviewPresentationCoordinator and presentation identities (ADR-003)
  |
  +-- M4-02  Engine thread and command queue (ADR-005)
        |
        +-- M4-03  Fake decoder/compositor ports and bounded scheduling (ADR-003/ADR-004)
              |
              +-- M4-04  Real Qt decode/composition adapter (ADR-005/ADR-007)
              |     |
              +-----+-- M4-05  MFC/Qt UI notification bridge (ADR-005/ADR-007)
                          |
                          +-- M4-06  Feature-flagged routing (ADR-007)
```

M4-04 and M4-05 both depend on M4-03 but not on each other; M4-06 needs both.

## M4-01 — PreviewPresentationCoordinator and presentation identities

Completes ADR-003's half of this milestone's value types: the coordinator
that owns "which frame does this viewport want now," separate from transport
authority. Still framework-neutral, fake-port-testable, no app-code changes.

**Scope**

- `PresentationSessionId`, `PresentationRequestId` (ADR-003's monotonic,
  non-wrapping, non-reused identities).
- `TransportPresentationIdentity`, `EditingPresentationIdentity`,
  `PresentationAuthority`; `SourcePresentationTarget`,
  `SequencePresentationTarget`, `PresentationTarget`;
  `FramePresentationRequest`.
- `PreviewPresentationCoordinator`, implementing the precedence policy
  exactly as ADR-003 states it (Playing/Seeking/Prerolling use transport
  presentation only; Stopped/Paused may show an editing-preview override;
  the next transport-changing command clears that override; Failed retains
  the last accepted frame; an explicit clear removes it).
- Request-identity bookkeeping only in this issue — no real frame candidate,
  decoder, or compositor yet (that starts in M4-03/M4-04).

**Non-goals**

- No `CompositedVideoFrame`/`PresentedPosition` frame-publication plumbing
  (needs a real or fake decoder/compositor — M4-03).
- No engine thread (M4-02).

**Done when**

- repeated scrubbing, clip selection, source changes, transport resumption,
  snapshot replacement, viewport clear, and shutdown each produce a new
  `PresentationRequestId` per ADR-003;
- the paused clip-selection path changes only presentation request identity
  — playhead, playback generation, and phase are provably unchanged;
- resuming transport supersedes an editing-preview request;
- a result from an older presentation session is rejected even when its
  numeric request ID collides with one in the new session.

Architecture: ADR-003 (presentation-identity criteria only — 9, 10; frame
publication criteria 11, 12 wait for M4-03/M4-04).

## M4-02 — Engine thread and command queue

The application's first real background thread. `PlaybackSession` moves onto
it; every command is serialized through a thread-safe queue instead of being
applied synchronously by the caller.

**Scope**

- A bounded or back-pressured engine command queue (ADR-005: "The command
  queue is bounded or back-pressured by policy... never executes user
  callbacks inline on a producer thread").
- One engine thread that owns `PlaybackSession` exclusively and applies
  queued commands in submission order.
- Explicit shutdown sequencing per ADR-005's order: submit shutdown, stop
  scheduling new work, wait for owned work, publish the final acknowledgment,
  close acceptance, join the thread.
- `PlaybackSession` itself does not change — this issue only moves who calls
  `applyCommand()` and when.

**Non-goals**

- No decoder/audio/compositor workers yet (M4-03/M4-04) — the queue has
  nothing to schedule besides `PlaybackSession` commands at this point.
- No UI notification delivery (M4-05).

**Done when**

- thread-contract tests prove commands are applied in submission order
  regardless of which thread submitted them;
- a test proves the engine thread is the only mutator of `PlaybackSession`
  state;
- shutdown tests prove the thread joins cleanly and no command submitted
  after shutdown is applied;
- command ordering remains observable at the engine boundary (ADR-005).

Architecture: ADR-005 (criteria 1 partial, 2, 3, 8 — audio/decoder-specific
criteria wait for M4-03/M4-04).

## M4-03 — Fake decoder/compositor ports and bounded scheduling

Implements ADR-003's bounded latest-wins video policy and ADR-004's anchor-
driven scheduling loop, entirely against fake `IVideoDecodeService`/
`IAudioDecodeService`/`IVideoCompositor` test doubles — proving the
coalescing and staleness rules deterministically before any real decoder
exists to make them flaky.

**Scope**

- `IVideoDecodeService`, `IAudioDecodeService`, `IVideoCompositor` port
  interfaces (ADR-007's framework-neutral ports).
- `PlaybackWorkIdentity`, `SequenceWorkIdentity`, `VideoDecodeRequest`,
  `DecodedVideoFrame`, `CompositedVideoFrame`, `PresentedPosition` and its
  two alternatives.
- The bounded latest-wins policy: at most one in-flight video
  decode/composition request, at most one newer pending request, a newer
  valid pending request replaces the older one, replacing pending work
  releases its resources immediately.
- Fake port implementations for tests (test-only code, not shipped in the
  public core library, matching M3-01's fake-clock precedent).
- Wires M4-01's coordinator to accept/reject frame candidates by the rule
  ADR-003 states (current presentation session, newest desired request,
  matching authority; transport candidates additionally need current
  session/generation; editing candidates need current sequence/revision).

**Non-goals**

- No real Qt/FFmpeg decoding (M4-04).
- No audio device callback or ring buffer (M4-04) — this issue proves the
  scheduling/coalescing policy, not real audio delivery.

**Done when**

- a newer valid pending request replaces an older one and releases its
  resources immediately;
- an in-flight fake result that arrives stale (wrong session/generation/
  revision) is discarded without a state transition or UI publication;
- frame pixels and position metadata are accepted and published atomically;
- `FramePresented` is distinguishable from decode/composition readiness and
  never advances transport.

Architecture: ADR-003 (criteria 6, 7, 11, 12, 13), ADR-004 (criterion 1
wiring the scheduler loop; audio/video-specific criteria wait for M4-04).

## M4-04 — Real Qt decode/composition adapter

*Higher risk than the issues above: this is the first place real
`QMediaPlayer`/`QVideoSink`/`QAudioOutput` behavior meets the new engine, and
some of what "correct" looks like (frame timing under real decode latency,
real audio-buffer underflow behavior) is only fully checkable by running the
app and looking at it — automated tests can prove the identity/ownership
rules but not that video looks right.*

**Scope**

- Concrete `IVideoDecodeService`/`IAudioDecodeService`/`IVideoCompositor`
  implementations backed by Qt Multimedia, compiled only under
  `MINI_EDITOR_USE_QT` (alongside the existing `QtMediaPlaybackBackend`).
- Decoder/compositor workers per ADR-005's ownership rules: they own only
  their own decoder objects and temporary resources, never touch widgets or
  editor containers, and never mutate `PlaybackSession`/the anchor directly.
- The audio callback boundary: non-blocking, no allocation, no engine-lock
  contention, never waits for video (ADR-004/ADR-005's real-time
  constraints).
- `IPlaybackClock`'s production implementation: `std::chrono::steady_clock`
  for milestone 1, exactly as ADR-004 specifies (the audio-device-clock
  exception is explicitly out of scope until a later ADR).

**Non-goals**

- No MFC/Qt UI notification wiring yet (M4-05) — decoded/composited frames
  reach the coordinator (M4-01/M4-03) but nothing routes them to a visible
  widget yet.
- No feature flag routing real timeline preview through this path yet
  (M4-06) — this issue proves the adapter against the core boundary, not
  against the live application.

**Done when**

- core ownership tests (ADR-005 criterion 11) still run without Qt Widgets,
  MFC, or hardware — only this adapter's own tests touch real Qt Multimedia;
- audio-callback tests prove no blocking, allocation, UI call, or
  engine-lock contention on the callback path;
- a validated decode failure transitions the session to
  `PlaybackPhase::Failed` with a `PlaybackError`; a stale failure does not.

Architecture: ADR-004 (criteria 2, 3, 6, 8, 9), ADR-005 (criteria 4, 5, 6, 7,
9), ADR-007 (Qt adapter section).

## M4-05 — MFC/Qt UI notification bridge

*Also higher risk: this is new code in `MainFrame`'s message loop, the one
class every build configuration shares.*

**Scope**

- `IPlaybackEventSink` (ADR-007's UI notification port) plus a thread-safe,
  non-blocking hand-off from the engine thread to the GUI thread.
- A new `WM_APP+n` message and `ON_MESSAGE` handler in `MainFrame` (there is
  no existing one to extend) that drains the notification queue on the GUI
  thread — added so it compiles and is inert in both build configurations
  until M4-06 turns the feature flag on.
- Qt widget updates continue to come from that same MFC-driven drain in the
  current coexistence shell, exactly as ADR-007's Qt adapter section
  specifies for this phase (a future Qt-native shell using queued signals
  instead is explicitly out of scope).
- `deleteLater()`/`QObject` thread-affinity discipline per ADR-005 for
  anything crossing into Qt-owned objects during the drain.

**Non-goals**

- Does not retire `MainFrame::OnTimer`/`kPlaybackTimerId` — that happens in
  Milestone 5 once the new path is proven equivalent.
- No behavior change to the visible application yet; the new message is
  never posted until M4-06's flag is on.

**Done when**

- shutdown tests prove queued work cannot call a destroyed receiver and
  every application-owned worker is joined before its owner is destroyed;
- a test or code-level check proves `deleteLater()` objects are deleted only
  by their owning event loop;
- MFC and Qt adapters can issue the same engine commands and consume the
  same immutable publications without duplicating transport state.

Architecture: ADR-005 (criteria 8, 9, 10), ADR-007 (MFC adapter and Qt
adapter sections).

## M4-06 — Feature-flagged routing

Wires everything above together behind a flag that defaults off.

**Design note requiring a decision, not just a name:** ADR-007 says "feature
flags select an adapter or preview surface at the application boundary," but
does not choose compile-time vs. runtime. This milestone's existing
convention is a CMake option (`MINI_EDITOR_USE_QT`) resolved at compile
time. This issue proposes following that precedent — a new compile-time flag
(default off) rather than a runtime toggle — because Milestone 5's own job is
"make it default," which reads as another compile-time flip once comparison
is complete, not a user-facing runtime setting. Flagged here rather than
decided silently; a runtime toggle would also satisfy every acceptance
criterion below if preferred.

**Scope**

- The flag routes timeline preview's commands to the new engine while
  leaving source preview on the existing `IPlaybackBackend` path, exactly as
  ADR-002/ADR-007 permit.
- At no point may both paths hold transport authority for the same preview
  session at once.

**Non-goals**

- Does not make the new path the default, compare regression behavior, or
  remove any legacy code — that is Milestone 5 in full.

**Done when**

- with the flag on, no legacy `EditorSession` playback mutator is called for
  timeline preview;
- with the flag off (the default), application behavior is byte-for-byte
  unchanged from before this milestone;
- a flag change cannot create two authorities for one preview session.

Architecture: ADR-002 (criterion 13, now reachable), ADR-007 (feature-flag
section).

## What remains deliberately deferred

Making the new path the default, removing MFC timer advancement, comparing
new-vs-legacy regression behavior, and retiring `IPlaybackBackend`/
`QtMediaPlaybackBackend` are Milestone 5. This milestone proves the new path
works and is inert by default; it does not switch anyone onto it.

## Human decision gates

M4-01 through M4-03 directly apply accepted ADR-003/ADR-004/ADR-005 text with
fake ports, the same shape of work as Milestones 1-3. M4-04 and M4-05 touch
real Qt Multimedia and MFC's message loop for the first time; their
acceptance criteria are testable, but "does decoded video actually look
right" is not something a unit test confirms, so treat a working build and
passing tests on those two issues as necessary, not sufficient, and expect
to actually run the application before considering them done. M4-06's
compile-time-vs-runtime flag choice is flagged above rather than decided
silently — proceeding with the compile-time default unless redirected. A new
product requirement, an additional transport/decoder command, or a change to
the accepted port interfaces pauses the automation and requires a human
decision.

## Agent handoff rule

An implementation agent may take exactly one ready issue. It reports the
changed files, build/test commands, and any decision gate it encountered.
The next ready issue starts only after its predecessor is reviewed and
merged, so `main` remains a known-good learning baseline.
