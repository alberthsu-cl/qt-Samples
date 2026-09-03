# ADR-007: Framework-Neutral Core and Qt/MFC UI Adapters

Status: Proposed

Date: 2026-09-03

## Context

The product is migrating its UI incrementally from MFC to Qt. ADR-002 through
ADR-006 define one engine authority, immutable snapshots, timing, thread
ownership, and project identity. Those contracts only remain reusable if the
core does not depend on Qt widgets, MFC windows, or a particular media backend.

The migration needs both MFC and Qt panels to issue identical playback commands
and display identical engine publications while the old and new UI coexist.
The existing MFC timer must not become a hidden dependency for engine progress,
and Qt event processing must not be pumped manually from that timer.

## Decision

The playback core is framework-neutral C++17. It contains the time/identity
types, project-ready values, resolver policy, `PlaybackSession`, scheduler,
and all command/result contracts. It depends only on standard C++ value types
and abstract ports:

```cpp
class IPlaybackClock;
class IVideoDecodeService;
class IAudioDecodeService;
class IVideoCompositor;
class IPlaybackEventSink;
```

Core interfaces accept and publish framework-neutral values. Qt/MFC objects,
`QObject*`, `QVideoFrame`, `QImage`, `HWND`, `CDC`, `CString`, and UI callbacks
do not appear in core headers, snapshots, resolver results, session state, or
engine command/event values.

Framework-specific media handles may exist inside a concrete decoder or
compositor adapter, but are converted to a framework-neutral buffer/descriptor
before crossing into the core. The core does not choose a GPU API; D3D11, QRhi,
and CPU implementations remain replaceable adapter details.

## Shared command and publication boundary

Both UI frameworks use the same immutable commands and publications already
defined by ADR-002 and ADR-003:

```cpp
using PlaybackCommand = std::variant<
    OpenSource, InstallSnapshot, Play, Pause, Stop, Seek, SetRate, Shutdown>;

using PlaybackEvent = std::variant<
    StatusChanged, PlaybackCommandRejected, VideoFrameReady,
    PlaybackEnded, MediaFailed>;
```

The current four-member `PlaybackCommand` enum in `ProjectState.h` is renamed
`LegacyPlaybackCommand` during coexistence. The eight-alternative engine
`PlaybackCommand` is the only command type at the new engine boundary. The
temporary `IPlaybackBackend` adapter translates legacy controls to the new
command route and is removed once the core-backed path becomes the default.

Every command/result carries the applicable identity from ADR-002/ADR-003.
Adapters first reduce framework-specific gestures to the shared
framework-neutral `EditorIntent`; shared translation maps that intent to the
appropriate `PlaybackCommand`. Adapters never modify an event to make it
current; they pass the immutable value to the core-side consumer that validates
identity.

`EditorIntent` represents user-facing editor actions. It must gain explicit
seek and rate intents as those controls migrate. `OpenSource`,
`InstallSnapshot`, and `Shutdown` are project/engine lifecycle commands sent
by their owning adapter or service, not synthetic UI intents; toggle playback
may resolve to `Play` or `Pause` from the last published phase.

The core exposes a thread-safe UI notification queue through the
`IPlaybackEventSink` port. It does not call UI code inline and does not require
a Qt or MFC event loop to advance playback.

## MFC adapter

The MFC adapter translates menu, keyboard, timeline, and project actions into
framework-neutral commands. It posts one Windows message to `MainFrame` when
the UI notification queue becomes non-empty; the GUI thread drains that queue
and updates MFC controls and any embedded Qt widgets.

The MFC timer may repaint a playhead using published `PlaybackStatus`, but it
does not advance transport time, drain decoder work as a correctness mechanism,
or call `QCoreApplication::processEvents()`.

## Qt adapter

The Qt adapter translates Qt signals, actions, and widgets into the same
framework-neutral commands. In the current MFC-hosted coexistence shell,
embedded Qt widgets are updated by the MFC notification-queue drain. In a
future Qt-native shell, Qt-facing results use queued signals to the Qt GUI
thread.
`QObject` thread affinity, queued value delivery, and `deleteLater()` follow
ADR-005; no Qt object is owned by the core.

A Qt panel may replace one MFC panel at a time. Replacing a panel does not
replace `PlaybackSession`, project state, clock policy, snapshot construction,
or decoder work identity.

## Preview presentation adapters

`PreviewPresentationCoordinator` remains framework-neutral. A presentation
adapter owns only the final UI-surface step:

- an MFC adapter paints an accepted immutable frame into its MFC preview
  surface;
- a Qt adapter presents the same accepted frame to a Qt preview surface;
- either adapter reports `FramePresented` only after its own surface commits
  the frame, without advancing transport.

Presentation adapters must preserve the request identity, authority, and
position metadata received from ADR-003. They do not seek, play, pause, change
the active sequence, or mutate editor selection as a side effect of painting.

## Feature flags and migration

Feature flags select an adapter or preview surface at the application boundary,
not an alternate playback state machine. A flag may route timeline preview to
the new core while legacy source preview remains on the existing backend, as
ADR-002 permits. At no point may a flag create two authorities for one preview
session.

This migration sequence is the adapter-level view that integrates, rather than
replaces, the finer migration strategies in ADR-002 through ADR-005:

1. Introduce core value types and fake ports with no visual behavior change.
2. Route a feature-flagged timeline preview through the core and notification
   bridge.
3. Compare MFC and Qt adapter behavior using the same regression scenarios.
4. Make the core-backed adapter the default and remove MFC-timer transport
   advancement.
5. Migrate remaining UI panels without changing core contracts.

## Acceptance criteria

ADR-007 is implemented when:

1. Core headers compile without Qt Widgets, MFC, codecs, graphics APIs, or
   platform window headers.
2. Core tests run with fake clock, decode, audio, compositor, and event-sink
   ports.
3. MFC and Qt adapters reduce the same user intent to the shared
   `EditorIntent`; framework-neutral tests verify the resulting
   `PlaybackCommand` mapping without requiring both full adapters in one
   feature-flagged binary.
4. Both adapters consume the same immutable `PlaybackStatus` and frame
   publication contracts without becoming transport authorities.
5. The engine progresses correctly when no MFC timer runs and without any call
   to `QCoreApplication::processEvents()` from MFC code.
6. Feature flags choose only adapters/surfaces; they do not select a second
   engine, clock, or playback-state machine.
7. A presentation adapter preserves frame identity and does not advance
   transport when it paints or acknowledges a frame.
8. `MiniEditorCoreTests` and the Qt widget test suite preserve existing
   timeline playback, pause, seek, end-of-timeline, and stale-result behavior
   across MFC and Qt panel replacement configurations.

## Consequences

The core can be tested independently and reused by MFC, Qt, future export, or
headless regression adapters. UI migration becomes a sequence of bounded
adapter replacements rather than a rewrite of playback authority. The cost is
explicit boundary code and value conversion, which makes framework coupling
visible and reviewable.

## Deferred decisions

This ADR does not choose a Qt rendering API, a D3D11/QRhi bridge, a concrete
codec backend, an MFC embedding technique, or a panel-by-panel UI migration
schedule. Those choices must preserve this adapter boundary.
