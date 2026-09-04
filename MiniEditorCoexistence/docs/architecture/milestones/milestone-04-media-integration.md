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
                          +-- M4-07  Media-failure observation path (ADR-002/ADR-005)
                                |
                                +-- M4-06  Feature-flagged routing (ADR-007)
```

M4-04 and M4-05 both depend on M4-03 but not on each other. M4-07 was added after
M4-04/M4-05 landed (a gap found in review: `QtPlaybackMediaWorker::mediaErrorOccurred`
was only logged, never routed into `PlaybackSession`'s Failed phase). M4-06 needs
M4-04, M4-05, M4-07, **and** the user's own manual smoke-test validation of
M4-04/M4-05 to all be complete before it starts.

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

## M4-07 — Media-failure observation path

Added after M4-04/M4-05 landed: `QtPlaybackMediaWorker::mediaErrorOccurred`
was only logged by the manual smoke test, never routed into
`PlaybackSession`'s Failed phase. ADR-002 requires a validated failure to
enter Failed and publish its error through the normal status/event path.

**Scope**

- `PlaybackEngine::reportFailure(PlaybackSessionId, PlaybackGeneration,
  PlaybackError)` — a new entry point that enqueues the observation into the
  *same* serialized queue `submit()` uses, callable from any thread, applied
  only on the engine thread, in submission order alongside ordinary
  commands. Not a direct call into `PlaybackSession` from outside the engine
  thread.
- The resulting status (Failed + error, or unchanged if the observation was
  stale) is published exactly like an applied command's status:
  `status()`/`drainRejections()`, and, if an `IPlaybackEventSink` is
  attached, pushed there too — no new channel.
- `EngineSmokeTestSession`'s `mediaErrorOccurred` handler now calls
  `engine_->reportFailure(...)` with the session/generation identity read
  from `engine_->status()` at report time, instead of only logging.
  `QtPlaybackMediaWorker` itself gained no new dependency on
  `PlaybackSession`/`PlaybackEngine` — it still only ever emits a plain Qt
  signal.

**Done when**

- a deterministic fake-port test proves a *current* failure observation
  (submitted through `PlaybackEngine::reportFailure()`, not by calling
  `PlaybackSession::reportFailure()` directly) transitions to Failed and is
  visible after the queue drains, and that its status is pushed to an
  attached event sink;
- the same test proves a *stale* (superseded-generation) failure observation
  is discarded without a phase change, and that a command submitted after it
  still applies in submission order;
- both Debug build trees (`vs2022-x64`, `vs2022-mfc-x64`) build and pass all
  tests.

Architecture: ADR-002 (failure/Failed-phase criteria), ADR-005 (a worker
never mutates `PlaybackSession` directly; the consumer validates identity).

## M4-06 — Feature-flagged routing

**Status: complete.** Happy path manually validated (project loaded, timeline
clip focused, Space plays real video in the standalone preview window);
failure path covered automatically by M4-08's end-to-end test. M4-07 and the
manual smoke-test validation of M4-04/M4-05 both completed first, as required
by this gate, and M4-08 (added after M4-06 landed) closed the last blocker.

**Bug found during that manual validation, and fixed:**
`updateTimelineEngineSnapshot()` passed `EditorSession::projectSnapshot()`
straight to the snapshot builder, but that function deliberately returns an
`EditorProject` with `mediaAssets` empty — it is the serialization-side view,
and `ProjectDocumentService` fills the library in separately. With no media
assets the builder rejected every snapshot
("A timeline clip has an invalid identity or media reference"), so nothing was
installed, the worker was never given a file, and Play appeared to do nothing
while routing itself was working correctly. Diagnostic logging added at every
silent branch is what pinpointed it; that logging stays.

Wires everything above together behind a flag that defaults off.

**Chosen scope (see also the design note below):** timeline routing uses its
own standalone preview window, not the app's real preview panel — the
milestone's own acceptance criteria (no legacy mutator, no dual authority,
off = unchanged) do not require live visual parity in the real panel, and
that work reads more like Milestone 5's "compare behavior" job. This was a
deliberate choice (asked of the user rather than decided silently) once
`QtMediaPlaybackBackend`'s shared `player_`/`timelineAudioPlayer_` design —
one `QMediaPlayer` already serving both source preview and timeline video —
turned redirecting the real preview panel's video sink into materially
higher-risk work than the alternative.

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

**Verification:** both build trees pass all tests with the flag at its OFF
default; a build with the flag on compiles, links, and passes the full test
suite. Manually confirmed by running the routing build: opening a project,
focusing a timeline clip, and pressing Space plays real video and audio in
the standalone "Timeline Preview (New Engine)" window, with the app's own
preview panel untouched.

**Scope note carried over from the router's own implementation:** only the
first video clip on V1 is opened for preview. Multi-clip timeline resolution
(switching source as the playhead crosses clip boundaries) needs
`TimelinePlaybackResolver` adapted to consume a snapshot — ADR-003 migration
step 3, still unstarted, out of scope here.

## M4-08 — Media-failure observation path for TimelineEngineRouter

**Status: complete.** Added after M4-06 landed, mirroring the exact gap
M4-07 closed for `EngineSmokeTestSession`: `TimelineEngineRouter` reuses
`QtPlaybackMediaWorker` but never connected its `mediaErrorOccurred` signal
to anything, so a real media failure during routed timeline preview was
silently dropped — the worker emitted the signal, `PlaybackSession` never
transitioned to Failed.

**Scope**

- `TimelineEngineRouter`'s constructor connects `mediaErrorOccurred` to a
  handler calling `engine_->reportFailure(...)`, reading the session/
  generation identity from `engine_->status()` at report time — the exact
  same pattern `EngineSmokeTestSession` already uses. No other routing code
  changed: `executeEditorCommand()`/`seekPreviewToCurrentFrame()`/
  `OnTimer()`'s gating, the single-clip preview scope, and
  `PlaybackEngine`/`PlaybackSession` themselves are all untouched.
- Strengthened `verifyPlaybackEngineFailureObservation()` with two scenarios
  modeling this exact call shape literally — one `status()` read, both
  identity fields destructured from that single snapshot, not read
  independently — proving a current snapshot's identity still transitions to
  Failed, and one captured before other commands ran (now stale) is
  discarded.
- The duplicated "read status, tag, enqueue" step in both Qt adapters was
  then extracted into a shared framework-neutral
  `reportWorkerFailure(PlaybackEngine &, std::string)`, so the call shape
  exists once and is covered once.

**Automated end-to-end verification.** Because this failure path produces
log/state output rather than visual or audio output, it is verified by test
rather than by hand:
`MiniEditorQtWidgetTests::workerMediaErrorEntersFailedThroughTheEngineQueue()`
writes an undecodable file to a `QTemporaryDir` (so an upstream
`std::filesystem::exists` check passes and decode is genuinely attempted),
opens it through a real `QtPlaybackMediaWorker` on its own thread, waits for
the real `QMediaPlayer` error, and asserts `PlaybackSession` reached Failed
with a non-empty error. Since `shutdownAndJoin()` drains the queue after the
already-queued observation, it also proves the failure travelled through the
engine's serialized queue rather than mutating the session directly. No GUI,
no saved project, no sample media involved.

Architecture: ADR-002 (failure/Failed-phase criteria), ADR-005 (a worker
never mutates `PlaybackSession` directly).

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
