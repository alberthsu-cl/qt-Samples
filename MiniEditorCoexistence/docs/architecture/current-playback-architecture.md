# Current Playback Architecture

Status: observed baseline before the timeline playback engine refactoring.

This document describes how playback works today. It is intentionally not a
proposal. Its purpose is to give future design work and GitHub issues one
shared, code-backed starting point.

The visible editor UI is frozen during this work unless an engine change needs
a small integration adjustment.

## Scope

This map covers:

- selected source-media preview;
- V1 video/still timeline preview;
- A1 audio timeline playback;
- transport commands, seeking, and playback-rate changes;
- the MFC/Qt message-loop boundary;
- state ownership and callback flow.

Thumbnail and waveform generation are separate preview-support services. They
use their own `QMediaPlayer` instances but are not part of transport playback.

## Runtime context

The executable is still an MFC application. `MiniEditorCoexistenceApp` owns the
normal application lifetime and message loop. In a Qt-enabled build,
`QtRuntime` creates the one required `QApplication`, with
`quitOnLastWindowClosed` disabled, before `MainFrame` constructs embedded Qt
widgets.

All application-owned playback objects are constructed on the main UI thread:

- `MainFrame`;
- `EditorSession`;
- `QtMediaPlaybackBackend`;
- its two `QMediaPlayer` objects and two `QAudioOutput` objects;
- `QtPreviewPanel` and its `QVideoSink`;
- transport and timeline widgets.

Qt Multimedia may use internal implementation threads, but the application has
not created an explicit playback worker thread. Signals connected to these
main-thread Qt objects are handled on their Qt thread affinity, which is the UI
thread.

During playback, the MFC timer periodically calls
`QCoreApplication::processEvents(...)`. This gives queued Qt Multimedia work a
short opportunity to reach the UI-thread objects while MFC remains the outer
message-loop owner.

## Main components

| Component | Current responsibility |
| --- | --- |
| `MainFrame` | Composition root. Connects MFC commands and Qt widget callbacks, owns the MFC playback timer, refreshes views, and creates seek requests. |
| `EditorSession` | Owns selection, one timeline model, source/timeline `PlaybackState`, edit state, history, and observer notification. |
| `EditorCommandController` | Routes framework-neutral editor intents to timeline editing or the playback backend. |
| `TimelineEditingController` | Coordinates focus, selection, timeline edits, duration changes, and seeks caused by editing gestures. |
| `TimelinePlaybackResolver` | Pure C++ mapping from timeline frame to active V1/A1 placements and source frames. |
| `PreviewStateResolver` | Chooses what the preview should show, including the special stopped-selection policy. |
| `MediaPlaybackPlanResolver` | Converts active editor state into desired source/V1 decoder state. |
| `TimelineAudioPlaybackPlanResolver` | Converts active timeline state into desired A1 decoder state. |
| `PreviewSeekRequestResolver` | Captures the current video and audio plans into an immutable seek request with an increasing request ID. |
| `QtMediaPlaybackBackend` | Reconciles plans with two `QMediaPlayer` objects, applies seeks/rate/mute/fades, and feeds decoded video to the preview sink. |
| `SimulatedPlaybackBackend` | Advances `EditorSession` by one timeline frame for still images, gaps, the MFC fallback, and current timeline playback. |
| `QtPreviewPanel` | Owns `QVideoSink`, receives decoded video frames, and paints decoded or fallback imagery. |
| `QtTransportPanel` | Displays playback state and emits transport, seek, and rate requests. It does not own playback. |

## Current data model

`EditorProject` contains one flat `timelineItems` collection. `EditorSession`
materializes that collection as one `TimelineModel`. There is no sequence ID,
active sequence, or collection of timelines yet.

`EditorSession` owns two transient playback states:

```text
sourcePlaybackState_    selected media-library asset
timelinePlaybackState_ active timeline preview
```

`isTimelineFocused_` selects which one is returned by `playbackState()` and
therefore which one most callers read or modify.

`PlaybackState` contains:

- playing and paused flags;
- current frame;
- fixed preview frame rate, currently 30 fps by default;
- duration in frames;
- transient playback-rate percentage.

The model uses plain `int` frame values for both timeline positions and source
positions. Their meaning depends on the field name and surrounding type; the
compiler cannot prevent mixing them.

## Framework-neutral timeline resolution

`TimelinePlaybackResolver` is the strongest reusable part of the current
design. Given a `TimelineModel`, `MediaLibrary`, and timeline frame, it returns:

- the active V1 placement, if present;
- the active A1 placement, if present;
- clip-local frame;
- source-media frame;
- clip/source durations;
- clip settings;
- evaluated fade gain.

For video and audio:

```text
source frame = source-in frame + (timeline frame - clip start frame)
```

For a still image, source frame remains zero while its timeline placement has a
duration.

The resolver does not depend on MFC, Qt, a decoder, or a widget. It is already
close to an engine-domain component. Its current limitation is that it reads
the mutable live `TimelineModel` and `MediaLibrary` rather than a versioned
playback snapshot.

## Source-preview flow

Source preview is selected when `EditorSession::isTimelineFocused()` is false.

```text
Media-library selection
    -> TimelineEditingController::selectSourceAsset(...)
    -> EditorSession selects source playback state
    -> MainFrame::synchronizePlaybackTimer()
    -> QtMediaPlaybackBackend::synchronize()
    -> MediaPlaybackPlanResolver
    -> QMediaPlayer load/seek/play/pause
    -> QVideoSink::videoFrameChanged
    -> QtPreviewPanel paints decoded frame
```

For real selected video/audio, `QMediaPlayer` is the source-position authority.
Its `positionChanged` and `durationChanged` signals call
`QtMediaPlaybackBackend::updateSessionFromPlayer()`, which writes the reported
frame/duration back into the active `EditorSession` playback state.

The resulting `EditorChange::Playback` notification returns through
`MainFrame::refreshEditorViews()` to the transport and preview widgets.

Still images and fake/unavailable media use `SimulatedPlaybackBackend`, whose
clock advances the session one frame per MFC timer tick.

### Source first-frame workaround

A paused `QMediaPlayer` may not decode a frame at position zero. The backend
therefore performs a silent preroll:

1. mute the player's audio output;
2. seek to the requested source frame;
3. briefly call `play()`;
4. wait for a matching `QVideoSink::videoFrameChanged` frame;
5. pause the player and restore the intended mute state.

This workaround is also used to display stopped or paused timeline frames.

## Timeline-preview flow

Timeline preview is selected when `EditorSession::isTimelineFocused()` is true.

```text
MFC playback timer
    -> MainFrame::OnTimer(...)
    -> QtMediaPlaybackBackend::advanceOneFrame()
    -> QCoreApplication::processEvents(...)
    -> SimulatedPlaybackBackend::advanceOneFrame()
    -> EditorSession::advancePlaybackFrame()
    -> TimelinePlaybackResolver resolves new V1/A1 sources
    -> QtMediaPlaybackBackend reconciles both QMediaPlayer objects
    -> views refresh from EditorSession notification
```

Unlike source preview, timeline position is not taken from `QMediaPlayer`.
`EditorSession` plus `SimulatedPlaybackBackend` and the MFC timer are the
timeline-head authority. This deliberately lets still images, gaps, video, and
audio advance through the same frame-indexed timeline.

The primary `QMediaPlayer` supplies decoded V1 video and its embedded audio.
When the resolver crosses a clip boundary, the backend may stop it, replace its
source, apply the resolved source-frame seek, and restart it.

The second `QMediaPlayer`, `timelineAudioPlayer_`, independently plays the A1
audio asset. Its position is initially aligned from the resolver's source
frame. Fade gain is applied by changing `timelineAudioOutput_` volume.

There is currently no common media clock measuring drift between these two
players and the MFC-driven timeline head.

## Seek flow

Timeline-ruler and transport-slider gestures first update editor state, then
request decoder synchronization:

```text
UI gesture
    -> TimelineEditingController updates focus/head
    -> EditorSession publishes Playback change
    -> MainFrame::seekPreviewToCurrentFrame()
    -> PreviewSeekRequestResolver snapshots current plans
    -> QtMediaPlaybackBackend::seek(request)
```

Each request receives a monotonically increasing `requestId` from `MainFrame`.
`PreviewSeekRequestTracker` accepts only newer requests and verifies that a
decoded video-frame completion belongs to the newest pending request.

This is an important existing stale-result contract. It protects explicit
video seeks, but it is not yet a general playback generation shared by all
video, audio, scheduling, and composition work.

## Transport command flow

Menus, keyboard shortcuts, and Qt transport controls become an `EditorIntent`.
`EditorCommandController` forwards playback intents to `IPlaybackBackend`.

For source media, the Qt backend changes both `EditorSession` state and the
primary `QMediaPlayer` state. For timeline playback, the backend changes the
session, then reconciles the primary and A1 players with the newly resolved
plans.

The backend returns `PlaybackClockAction::EnsureRunning` or `Stop`.
`MainFrame` translates that policy into MFC `SetTimer`/`KillTimer` calls.

Playback rate is one transient value copied into both source and timeline
states. The Qt backend applies it to both media players, and the simulated
clock changes the MFC timer interval accordingly.

## View-update flow

`EditorSession` synchronously notifies its observers after state changes.
`MainFrame` is the registered observer and resolves the latest data for all
views.

For playback-related changes it updates:

- preview playback state;
- transport playback state;
- preview content via `PreviewStateResolver`;
- timeline presentation/playhead;
- status text.

The decoded frame itself arrives separately through `QVideoSink`. Consequently
the model/playhead update and picture update are not one atomic publication.

## Current authority and ownership

| Concern | Current authority/owner | Observation |
| --- | --- | --- |
| Editable project/timeline | `EditorSession` and `TimelineModel` | One mutable global timeline. |
| Active preview context | `EditorSession::isTimelineFocused_` | Selection/focus also chooses which playback state is active. |
| Source playback position | Primary `QMediaPlayer`, copied into `EditorSession` | Real media only. |
| Timeline playback position | `EditorSession`, advanced by MFC timer | Independent of player timestamps. |
| V1 decoded picture | Primary `QMediaPlayer` -> `QVideoSink` | Same player is reused for source and timeline contexts. |
| V1 embedded audio | Primary `QMediaPlayer`/`QAudioOutput` | Muted independently through project mix state and silent-preroll policy. |
| A1 audio | Second `QMediaPlayer`/`QAudioOutput` | Started and sought from the same plan time, but not continuously clock-synchronized. |
| Timer lifetime | `MainFrame` | Backend returns timer policy; MFC performs it. |
| UI presentation | `MainFrame` plus Qt/MFC hosts | Refreshed synchronously from session notifications. |
| Explicit seek staleness | `PreviewSeekRequestTracker` | Request-ID policy currently focuses on decoded video seek completion. |

## What should be preserved

The replacement engine should preserve or evolve these successful boundaries:

- framework-neutral playback interfaces;
- pure timeline-to-source resolution;
- stable media and timeline-clip IDs;
- explicit source versus timeline preview context;
- immutable explicit seek intent;
- monotonically increasing identity for rejecting stale results;
- UI widgets as presenters/command sources rather than playback owners;
- one shared playback-rate preference across video and audio preview.

## Architectural limitations to address

### Split playback authority

Source position is player-driven, while timeline position is MFC-timer-driven.
The meaning of “current playback time” changes with preview context.

### Focus and playback context are coupled

`isTimelineFocused_` selects the active playback state. Editing focus,
selection, and playback-session identity therefore influence one another.

### Live mutable state is repeatedly resolved

Playback plans read `EditorSession`, `TimelineModel`, and `MediaLibrary`
directly. There is no immutable, revisioned timeline snapshot shared by all
playback work.

### No explicit application decode workers

The application has no decode-service thread ownership, request queue,
cancellation protocol, or bounded frame queue. It relies on `QMediaPlayer`'s
high-level playback behavior and internal implementation.

### No common A/V synchronization policy

The MFC timer, V1 player, and A1 player are started from related positions but
are not governed by one measured master clock with drift correction, frame
drop, or audio-buffer policy.

### Frame types do not encode their domain

Timeline frame, clip-local frame, and source frame are all `int`. Correctness
depends on naming and convention rather than C++ type checking.

### One global timeline is embedded in project/session APIs

`EditorProject`, `EditorSession`, plan resolvers, and the playback backend all
assume one `TimelineModel`. Multiple sequences would currently require broad
changes.

### Presentation is not atomic

Playback state and decoded pictures arrive through different paths. This makes
stale callback filtering and generation ownership important during seek,
selection changes, and source replacement.

### Event pumping is tied to the playback timer

The MFC timer also serves as a short Qt event-processing opportunity during
playback and silent preroll. Timer policy and cross-framework event delivery
are therefore partially coupled.

## Existing behavioral invariants

The current tests and code establish behavior that the new engine must either
preserve or intentionally revise:

- source and timeline previews keep independent positions;
- changing focus selects the corresponding preview context;
- pause retains the displayed frame;
- stop returns to frame zero;
- completed timeline playback returns to frame zero;
- completed source playback retains its last frame in paused state;
- trimmed clips map timeline time through `sourceInFrame`;
- still images always use source frame zero;
- V1 and A1 can be active at the same timeline position;
- gaps contain no resolved media but the timeline clock can continue;
- hidden A1 is not auditioned;
- V1 embedded audio can be muted independently;
- playback speed is shared by V1 and A1;
- a newer explicit seek supersedes an older seek;
- stopped selection preview may show the selected clip's first frame even when
  the timeline head is elsewhere;
- paused or playing timeline preview follows the timeline head.

## Questions for the target design

The next architecture document must decide, before implementation issues are
dispatched:

1. What strongly typed representation distinguishes timeline, source, and
   presentation time?
2. Which object is the sole playback-state authority?
3. Is audio always the master clock when present, and what clock is used for
   silent timelines?
4. What immutable snapshot is handed to playback, and when is its generation
   invalidated?
5. What is the threading and lifetime contract for video/audio decode
   services?
6. What queue sizes and stale-result rules apply after seek or source change?
7. Where does composition occur, and what frame type crosses that boundary?
8. How does a playback session identify its sequence without relying on UI
   focus?
9. Which current stopped-selection behavior belongs to editor UX rather than
   playback-engine policy?
10. How can the current UI consume the new engine without visible redesign?

## Reading guide

When reviewing this map, concentrate on three distinctions:

1. **Ownership:** who is allowed to change a value?
2. **Authority:** whose time or state is considered correct when components
   disagree?
3. **Thread affinity:** on which thread may an object be called or destroyed?

The current implementation often has clear ownership but conditional
authority. That is the central problem the new timeline engine must solve.
