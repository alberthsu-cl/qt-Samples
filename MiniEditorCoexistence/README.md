# Mini Editor Coexistence

This sample models the **UI structure** of a desktop video editor and now has
an intentionally small real source-media playback path. It is a learning
project for gradual MFC-to-Qt migration, not a video editor implementation.

## Phase 0 — Pure MFC baseline

The application contains four focused custom MFC child windows:

```text
Media Library | Preview | Properties
--------------+---------+-----------
Timeline
```

- **Media Library** shows generated video, image, and audio asset tiles.
- Clicking an asset updates the preview, properties, timeline clip, and MFC
  status bar.
- **Preview** preserves a 16:9 rectangle but displays a generated color
  placeholder instead of decoding video.
- **Timeline** is static custom painting; it is intentionally not yet a real
  edit/drag/drop control.

`DemoProject.h` is framework-neutral and owns the sample asset data. It will
be the stable data boundary when Phase 1 replaces only the Media Library with
a Qt Model/View panel.

`MfcEditorPaneBase` contains only shared MFC painting mechanics.
`MfcMediaLibraryPane`, `MfcPreviewCanvas`, `MfcPropertiesPane`, and
`MfcTimelinePane` each own their own drawing
and input behavior. This gives the migration a one-to-one seam: the Phase 1
Qt Media Library can replace `MfcMediaLibraryPane` without turning a large
`switch` statement into a permanent dependency.

## Phase 1 — Qt Media Library

By default, CMake builds the first coexistence migration:

```text
Qt Media Library | MFC Preview | MFC Properties
-----------------+-------------+----------------
MFC Timeline
```

`MediaAssetModel` is a `QAbstractListModel` adapter over the existing
framework-neutral `DemoProject.h` data. `QtMediaLibraryPanel` uses `QListView`
in icon mode plus a custom `QStyledItemDelegate` to paint thumbnail tiles.
`QtMediaLibraryHost` makes the native MFC/Qt HWND boundary explicit.

When a Qt tile is selected, the panel emits `assetSelected(int)`. MFC receives
that asset index and updates the unchanged MFC preview, properties, timeline,
and status bar. The selection index is intentionally the only value crossing
the UI-framework boundary.

## Phase 2 — Qt Properties panel

Phase 2 replaces the inspector as well:

```text
Qt Media Library | MFC Preview | Qt Properties
-----------------+-------------+--------------
MFC Timeline
```

`ProjectState.h` defines framework-neutral `ClipSettings`: opacity, scale, and
position. `MainFrame` owns one value per demo asset. `QtPropertiesPanel` uses
`QFormLayout`, sliders, spin boxes, and a combo box to edit those values. It
emits plain values back to MFC through `QtPropertiesHost`; MFC stores them and
redraws its unchanged Preview and Timeline panes.

When MFC changes selection or synchronizes stored settings, the Qt controls
are updated under `QSignalBlocker`. That prevents a model-to-view update from
being mistaken for a second user edit returning to MFC.

`MINI_EDITOR_USE_QT` is the one migration switch. Set it to `OFF` in the
CMake cache to rebuild the complete Phase 0 pure-MFC UI; set it to `ON` to
enable every panel migrated to Qt so far.

The Qt-enabled target contains only MFC infrastructure that it still uses:
`MfcWorkspaceSplitter` and the MFC frame/menu/status-bar shell. Complete MFC
fallback panes, including `MfcPreviewCanvas`, are included only in
the `MINI_EDITOR_USE_QT=OFF` target, so they do not distract from the active
coexistence project in Visual Studio.

## Phase 3 — Qt transport controls

Phase 3 leaves the preview-rendering surface on MFC and replaces only its
transport region:

```text
Qt Media Library | MFC Preview Canvas + Qt Transport | Qt Properties
-----------------+-----------------------------------+--------------
MFC Timeline
```

`MfcPreviewCanvas` represents an existing native/GPU preview surface in a
real editor. `MfcTransportBar` is the complete MFC fallback; when
`MINI_EDITOR_USE_QT=ON`, `QtTransportPanel` replaces only that bar through
`QtTransportHost`.

The Qt buttons emit framework-neutral `PlaybackCommand` values. `MainFrame`
owns `PlaybackState`, handles Play/Pause, Stop, and frame-step commands, and
uses a simple MFC timer to advance the sample playhead. It then synchronizes
the Qt timecode/button state and redraws the MFC Preview Canvas and Timeline.
In a production editor, the media engine would provide the playhead clock;
the UI boundary remains the same.

## Phase 5 — Qt timeline toolbar

The custom timeline canvas remains MFC, while the standard timeline controls
above it are now Qt:

```text
Qt Timeline toolbar (zoom, fit, ripple editing)
MFC Timeline canvas (ruler, tracks, clip, playhead)
```

`TimelineViewState` is framework-neutral and owned by `MainFrame`.
`QtTimelineToolbar` emits zoom, audio-track, and fit requests through its
host. `MfcTimelineCanvas` reads the resulting state and redraws itself.
This is the recommended incremental path for an established video-editor
timeline: migrate ordinary controls first, then tackle the complex custom
canvas only when its input and rendering design are ready.

## Phase 6 — Framework-neutral JSON workspace settings

The app saves user workspace preferences to:

```text
%LOCALAPPDATA%\QtLearningSamples\MiniEditorCoexistence\workspace.json
```

`WorkspaceSettingsStore` is standard C++/Win32 code, not Qt or MFC code. It
restores splitter sizes, selected media, timeline zoom, and audio-track
visibility before child controls are constructed. It saves the latest values
when the main frame closes. A video-editor project file would be separate:
this JSON represents one user's workspace, not edit decisions that travel
with a project.

## Phase 7 — MFC timeline seek interaction

Clicking the ruler on `MfcTimelineCanvas` converts a pixel position to a
zoom-aware frame number. The canvas emits that number through a plain C++
callback; `MainFrame` owns the `PlaybackState` update, then redraws the MFC
preview/canvas and synchronizes the Qt transport timecode. This preserves the
important boundary: the custom surface understands pixels, while the app
understands editor state.

## Phase 8 — Framework-neutral EditorSession

`EditorSession` now owns selection, per-clip settings, playback state, and
timeline view state. It exposes editor commands such as selection, seeking,
playback, and zoom, but has no MFC or Qt dependency. `MainFrame` keeps native
window layout and registers one state-change callback that refreshes both MFC
and Qt views. This is the important migration seam: UI controls request
changes; the session owns state; views render the resulting state.

## Phase 9 — Framework-neutral WorkspaceLayout

`WorkspaceLayout` owns splitter state, minimum dimensions, clamping, and all
child-rectangle calculations. It returns plain `WorkspaceRect` values, so it
has no MFC or Qt dependency. `MainFrame` now handles only the MFC-specific
status-bar boundary and applies the calculated rectangles to native MFC
windows and embedded Qt HWNDs. `WorkspaceLayoutState` is the layout portion
saved to the workspace JSON file.

## Phase 10 — Core tests and the `check` target

`MiniEditorCoreTests` is a small console test executable for the
framework-neutral `EditorSession` and `WorkspaceLayout` classes. It has no Qt
or MFC dependency. CTest registers it as `MiniEditorCoreTests`.

Normal application builds do **not** run tests. Build the CMake target named
`check` when you want one local validation command: it builds the app and the
test executable, then runs CTest for the active Debug/Release configuration.

## Phase 11 — Typed editor change notifications

`EditorSession` supports multiple plain-C++ observers and reports an
`EditorChange` bitmask: selection, clip settings, playback, or timeline view.
`MainFrame` subscribes while it is alive and refreshes only the affected MFC
and Qt views. For example, a playback timer tick updates the preview, timeline
canvas, transport, and status bar—not the media library or properties panel.

## Phase 13 — Framework-neutral Undo/Redo

`EditorHistory` owns the undo and redo stacks as framework-neutral Command
objects. `SourceClipSettingsCommand`, `SourceTimelineStateCommand`, and
`TimelineClipSettingsCommand` restore focused value edits. Structural editing
uses one `TimelineSnapshotCommand`: add, move, trim, Ripple, Delete, and Split
all store a valid before/after timeline plus focus state, so a multi-clip edit
remains one atomic command without adding another switch case.

`EditorSession` executes editing policy and records completed commands, but it
no longer contains an undo/redo type switch. During Undo or Redo it lends the
command a narrow `EditorCommandContext` containing only state references; the
command has no MFC, Qt, or `EditorSession` dependency. Playback, timeline view,
and workspace layout remain transient and are not recorded. The MFC Edit menu
continues to invoke the same framework-neutral `undo()` and `redo()` API.

## Phase 14 — Undoable timeline clip moves

Drag the selected clip horizontally in `MfcTimelineCanvas`, then release the
mouse to commit one `TimelineClipState` edit. The canvas owns only the pixel
coordinate conversion and temporary drag preview; `EditorSession` owns the
start-frame state and records one Undo/Redo history entry on release. Seeking
on the ruler remains a playback action and is intentionally not undoable.

The MFC menu also has real keyboard handling: **Space** toggles playback,
**Ctrl+Z** invokes Undo, and **Ctrl+Y** invokes Redo. The resource accelerator
table documents these shortcuts; `MainFrame::PreTranslateMessage()` performs
the translation because this sample creates its frame with `CFrameWnd::Create`
rather than MFC's resource-loading `LoadFrame()` path.

## Phase 15 — Portable project documents

`EditorProject` contains actual editing decisions: each catalog item's clip
settings and timeline placement. `ProjectSerializer` reads and writes that
framework-neutral data as a readable `.mini-editor.json` file. It deliberately
does **not** include workspace preferences such as splitters, selection, or
zoom; `WorkspaceSettingsStore` continues to retain those separately for the
current user and machine.

The File menu now supplies New, Open, Save, and Save As commands. `MainFrame`
only presents MFC file dialogs and messages; the serializer and the session
know nothing about MFC or Qt. Loading a project clears Undo/Redo history and
restores every editable clip state in one session notification.

Project format v6 stores `sourceInFrame` for each timeline placement. Video
and audio clips therefore retain their source range across Save/Open and
Undo/Redo. Version 5 projects migrate with source-in zero. Still-image
placements keep source-in at zero because their duration describes how long
the image appears on the timeline rather than a range of running source media.

## Phase 16 — Document dirty state

`EditorSession` tracks whether project edits are unsaved. New/Open/Exit now
ask whether to Save, Discard, or Cancel before replacing or closing a dirty
project. Save clears the dirty state, the title displays `*` while modified,
and the Save command is disabled when there is nothing to save.

## Phase 17 — Playback preview overlay

While playback is active, `MfcPreviewCanvas` draws a centered time overlay in
the preview: current time followed by the sample duration. It is rendered by
the existing double-buffered MFC canvas, so the overlay updates with the same
flicker-free timer repaint as the preview and timeline playhead.

## Phase 18 — Qt timeline canvas

The Qt-enabled build now uses `QtTimelineCanvas`, a QWidget that paints the
ruler, clip, audio track, and playhead with QPainter. It handles ruler seeking
and clip dragging through plain C++ callbacks. `MainFrame` still owns the
native layout and `EditorSession` still owns all state and Undo/Redo history.

Audio-track visibility belongs to the `A1` lane itself: its header contains a
small open/closed-eye toggle. The button updates the framework-neutral
`TimelineViewState`; hiding A1 suppresses audio-clip drawing, selection, and
audio drops, without changing the audio media or playback model.

The pure-MFC configuration retains `MfcTimelineCanvas`, making the two
implementations easy to compare while studying the migration boundary.

`TimelineGeometry` now owns the fixed V1/A1 coordinate policy independently
of Qt and MFC: zoom-aware frame/pixel conversion, ruler seeking, track and clip
rectangles, canvas sizing, and topmost overlap hit-testing. `QtTimelineCanvas`
converts these plain rectangles to `QRect` and concentrates on event handling
and painting. Pure C++ tests cover the geometry without creating a window.

The selected Qt timeline clip can be trimmed from either edge. The handles are
intentionally **not painted**: the focus frame already identifies the selected
clip, and the horizontal-resize cursor already announces that an edge is
grabbable, so drawn handles would only cover the clip's own content.
`kTrimHandleWidth` therefore describes an invisible hit zone shared by
`hitTestClip()` and the cursor update. Dragging an edge paints a live
provisional range, and releasing commits one `EditorSession` command for
Undo/Redo. The
framework-neutral `TimelineClipEdit` applies media-specific rules: video and
audio handles update source-in/out within the source duration, while a still
image's handles adjust when and how long it appears without a source limit.
The live range label reports source frames for timed media and display frames
for still images. Each drag readout is painted in the colour of the edit it
reports - amber for a trim, matching the focus frame, and blue for a media drop,
matching the insertion guide. They were previously near-black panels, which
disappeared into the dark timeline they float over.

Because this learning project intentionally supports only one video track and
one audio track, clips on the same track no longer overlap. The framework-neutral
`TimelineTrackPolicy` finds the nearest available gap for drops and moves, and
limits trim handles at adjacent clip boundaries. `TimelineModel` independently
enforces that invariant, while V1 and A1 may still contain media at the same time.
The Qt canvas adds an eight-pixel magnetic zone around frame zero and adjacent
clip edges. The tolerance is converted to frames at the current zoom level, so
clips move freely inside a gap and snap only when their edges are visually near.

The checkable **Ripple** toolbar mode makes structural edits close or open time
on only the edited track. Insertion shifts clips at and after the insertion
point, whole-clip moves close the old gap and open the destination, right/left
trim shifts following clips by the duration change, and Delete closes the
removed duration. Each multi-clip result is stored as one atomic timeline
snapshot for Undo/Redo; V1 edits never move A1 clips and vice versa. Ripple is
a workspace preference rather than portable project content.

The **Split** toolbar command and **Ctrl+B** divide the focused clip at the red
timeline cursor. The original ID remains on the left and a stable new ID is
created on the right. Video/audio advance the right clip's source-in by the
left duration; still images keep source-in zero. Media identity and placement
properties are copied, total timeline duration is unchanged, and the complete
operation—including selection of the right piece—is one Undo/Redo command.

The Edit menu also provides an internal timeline clipboard: **Ctrl+C** copies
the focused placement, **Ctrl+X** cuts it, **Ctrl+V** pastes at the red head,
and **Ctrl+D** duplicates after the focused clip. It is intentionally not the
Windows clipboard. Copies retain media ID, track, source-in/duration, and all
placement properties while receiving a new stable clip ID. Normal mode chooses
the nearest non-overlapping position; Ripple mode opens the copied duration on
that track. Cut, Paste, and Duplicate each record one `TimelineSnapshotCommand`.

Timeline focus is modeled separately from timeline clip selection. Clicking
an empty track area clears the clip ID but keeps Timeline Preview active,
clears the media-library highlight, and resets playback to frame zero. Deleting
the focused clip produces the same state. Clicking the ruler instead moves the
timeline head and focuses the V1 clip at that frame, or the A1 clip when V1 is
empty. A gap leaves the timeline focused without a clip. Selecting a clip no
longer resets the head, so the same ruler position can immediately enable
Split. Playback therefore never falls back to the first source-library asset.

`TimelinePlaybackResolver` completes the source-aware path independently of the
UI frameworks. For each playhead position it resolves the active V1 and A1
placements and maps timeline time to clip-local and source-media frames. Video
and audio use `sourceInFrame + clipLocalFrame`; still images keep source frame
zero while their display frame advances. Gaps deliberately resolve to no video.
Pause and frame stepping retain the resolved playhead frame and keep the
timeline/source information overlay visible. Natural **timeline** completion
stops and returns the head to frame zero, ready for the next Play command;
source preview still retains its final frame for inspection. An explicit Stop
also returns Preview to its stopped/focused state and hides the overlay.

## Phase 19 — Timeline model foundation

`TimelineModel` is the first step toward a real editor timeline. It starts
empty and stores independent `TimelineClip` records, each with its own ID,
source media-asset index, and timing. This allows the same source asset to be
placed multiple times. The current compatibility UI still uses its older
single-clip adapter; the next step will connect media-library drag/drop to
`TimelineModel` and then remove that adapter.

`TimelineClip` now carries `TimelineTrackType`. Dropped video and image assets
go to V1, while audio assets go to A1; both lanes remain empty until a clip is
dropped. The model test verifies that the same timeline can contain both track
types.

`TimelineModel::durationFrames()` now keeps a 600-frame minimum but extends
automatically to the end of the furthest clip. The hardcoded range is becoming
model data; horizontal scrolling is the next UI step needed to view longer
projects comfortably.

Each demo asset now has a numeric timeline duration. Video and audio retain
their catalog duration; still images use a 90-frame (three-second) default.
Dropped clip widths are therefore proportional to the source duration.

The timeline ruler now derives labels from frame position at 30 FPS and shows
timecode such as `00:00`, `00:02`, and `00:04`; the old pixel-based placeholder
numbers were removed.

## Phase 21 — Real media-library foundation

`MediaLibrary` is a framework-neutral source-asset collection with stable IDs.
It imports supported video, audio, and image file paths, infers their kind,
assigns sensible default timeline durations, and supports removal without
changing any other asset ID. Timeline clips will use these stable IDs rather
than mutable library rows in the next integration step.

Selecting a Qt timeline clip and pressing Delete now removes it through
`EditorSession`. Insertion, movement, and deletion all participate in the
same Undo/Redo history and preserve the clip ID when restored.

The Qt media library provides a drag source, and the Qt timeline accepts its
asset MIME type. During a drag, the pointer sits on the thumbnail's left edge
and the timeline paints a duration-sized ghost clip, a blue insertion guide,
and the proposed start time. These all identify the exact `startFrame` that
will be stored when the clip is dropped. `EditorSession` owns the insertion,
Undo/Redo history, dirty state, and serialized placement.

The default workspace reserves enough width for two media-library cards and
complete Properties editors. The timeline also reserves enough height for its
toolbar, both tracks, and the horizontal scroll bar. Persisted splitter values
are still restored, but the layout policy clamps older narrow values to these
usable minimums.

When timeline playback is stopped, Preview shows the focused video placement,
so Properties changes are visible immediately even if the playhead is outside
that clip. During playback, Preview returns to playhead-based clip resolution
and follows the complete timeline sequence.

An empty Qt timeline now shows a drag-and-drop hint instead of drawing the
legacy default asset. The pure-MFC fallback intentionally remains unchanged
until it is migrated to the new timeline model.

## Application-layer timeline controller

`TimelineEditingController` is the framework-neutral coordinator between UI
intent and `EditorSession`. It owns the application rules for clip/frame
focus, source-asset lookup, preview-duration synchronization, Split, Delete,
and the internal timeline clipboard. `MainFrame` now forwards MFC and Qt
callbacks to this controller instead of duplicating those policies beside
native-window and timer code.

The controller is compiled into `MiniEditorCoreTests`, so these interaction
rules can be exercised without constructing an MFC frame or Qt widget. This
is also a scalable migration seam: a future all-Qt main window can reuse the
same controller and session rather than reimplementing editor behavior.

## Framework-neutral preview presentation

`PreviewStateResolver` moves preview-selection policy out of `MainFrame`.
Given only `EditorSession` and `MediaLibrary`, it chooses source or timeline
mode, maps the playhead to V1/A1 source frames, handles gaps, and preserves the
stopped focused-clip preview behavior. `MfcPreviewCanvas` remains only the MFC
renderer of the resulting `PreviewState`; a future Qt preview can reuse this
same resolver and state contract. Core tests cover source video, still images,
stopped focused clips, paused timeline playback, and timeline gaps.

## Framework-neutral project documents

`ProjectDocumentService` coordinates `EditorSession`, `MediaLibrary`, and
`ProjectSerializer` without MFC or Qt dependencies. It provides New, Save,
Load, Import, and Remove operations as plain `ProjectDocumentResult` values.
The UI still owns its native file dialogs, confirmation prompts, and error
messages. New Project now restores both the default sample catalog and default
editing state, while Remove prevents an asset from being deleted while a
timeline placement still references it.

## Qt preview panel

The Qt-enabled build now hosts `QtPreviewPanel` through `QtPreviewHost`. It
renders the same learning placeholder—thumbnail color, placement settings,
timecode, and playback diagnostics—as the MFC preview, but consumes only
`PreviewState` and `PlaybackState`. `MfcPreviewCanvas` and its MFC paint helper
are now compiled only by the pure-MFC fallback configuration. This is the
intended migration result: one presentation contract, two temporarily
independent renderers, and no Qt preview code coupled to MFC drawing types.

## Incremental real media playback

`IPlaybackBackend` keeps transport intent independent of the playback engine.
The pure-MFC build uses `SimulatedPlaybackBackend`. The Qt-enabled build uses
`QtMediaPlaybackBackend`. An existing selected video/audio file is routed
through `QMediaPlayer` and `QAudioOutput`. Decoded video is presented by the
`QVideoWidget` owned by `QtPreviewPanel`.

The source player publishes its decoded position and duration back into the
active `PlaybackState`; existing Qt transport controls therefore display the
real media clock without owning the player. The MFC timer remains only as the
outer-loop bridge that lets queued Qt Multimedia callbacks reach the UI thread.
It does not advance frames while a real video source is playing.

For timeline playback, the framework-neutral `TimelinePlaybackResolver` maps
the timeline playhead to its active video placement and source-media frame.
`QtMediaPlaybackBackend` then loads or seeks that file, and switches the player
at a clip boundary. Still-image placements and empty timeline gaps deliberately
keep using the simulated timeline clock; this makes the current scope explicit.
The V1 track-header speaker independently mutes or enables the embedded audio
from video clips, while the A1 track continues through its separate player and
output. Applying opacity/position/fade effects to decoded video remains a later
rendering phase.

## Framework-neutral editor commands

`EditorCommandController` maps intent-level commands—Undo, Redo, timeline
clipboard actions, Split, and playback—to `EditorSession` and
`TimelineEditingController`. `MainFrame` maps its MFC menu IDs, accelerator
commands, Qt Transport commands, and the Qt Split button into this one API.
The controller returns whether its timer host must synchronize.

`PlaybackClockController` then expresses the timer policy as framework-neutral
`EnsureRunning` or `Stop` actions. `MainFrame` remains the MFC timer host for
now, but a later Qt-only shell can map the same actions to `QTimer` without
copying frame-advance or end-of-playback logic.

## Phase 12 — Flicker-free MFC playback painting

`MfcDoubleBufferedPaint` draws MFC preview and timeline content to a
compatible off-screen bitmap, then copies the completed frame to the window
with one `BitBlt`. Playback invalidation uses `Invalidate(FALSE)`, and the
paint handlers suppress the redundant `WM_ERASEBKGND` pass. This prevents the
background from becoming visible between frequent playback repaints.

## Phase 22 — Clip fades

`ClipFade` is the framework-neutral fade policy. `ClipSettings` gains
`fadeInFrames` and `fadeOutFrames`, so a fade is an edit decision that travels
with the placement through Undo/Redo, the internal clipboard, and the project
file. Nothing about a ramp lives in a renderer.

Three surfaces ask `ClipFade` the same question and therefore cannot disagree:

- `TimelinePlaybackResolver` reports `fadeGainPercent` for the resolved V1 and
  A1 media at the playhead.
- `PreviewStateResolver` folds the video gain into `effectiveOpacityPercent`
  and passes the audio gain through. `QtPreviewPanel` and the MFC fallback
  preview both render that one value.
- `QtTimelineCanvas` draws each clip's ramp triangles from the same normalized
  lengths, so the overlay always matches what the preview shows.

Two rules keep an impossible fade impossible:

- `QtPropertiesPanel` gives each fade a slider plus a spin box, exactly like
  Opacity and Scale. Both editors span the whole clip, so neither control ever
  stops responding; when the two ramps would overlap, the fade *not* being
  dragged yields. The panel receives the placement length through
  `setClipDurationFrames()`, which `MainFrame` also refreshes on
  `EditorChange::TimelineClip` because a trim changes the room available for
  fades.
- `ClipFade::normalize()` still runs at render time. A trim shortens a clip
  without touching its settings, so the two stored ramps are shortened
  proportionally until they exactly meet inside the clip.

While playback is stopped, the focused clip is an edit target rather than a
rendered timeline frame, so `PreviewStateResolver` deliberately suppresses its
fade. Otherwise a fade-in would hide the very placement being adjusted.

### Qt lesson: stylesheet vs. style vs. palette

The first version of these controls used only spin boxes, and their up/down
arrows did not work. The cause is worth remembering: applying **any**
stylesheet rule to a `QSpinBox` switches that widget to the stylesheet style,
which then draws the steppers exclusively from `::up-button` / `::down-button`
rules — and draws no arrow at all unless an `image:` is supplied. The Opacity
and Scale boxes hid the problem because their sliders did the real work.

The fix is to pick the right tool per widget. The spin boxes are now left out
of the panel stylesheet and instead get the **Fusion** style plus a dark
**palette**: Fusion honours palettes, so the fields stay dark while the real
style keeps drawing working steppers. `QComboBox` keeps its stylesheet rule,
because its arrow is not an interactive target on its own.

The spin boxes also use `setKeyboardTracking(false)`, so typing `120` is one
edit and one Undo entry instead of three.

Project format v9 stores both the existing per-placement fade/DSP settings and
the V1 embedded-audio mute state. Version 6 and older documents load with no
fades; version 7 and older documents load with no DSP effect; version 8 and
older documents load with V1 audio enabled. Every loaded placement is
re-clamped to its own duration.

## Phase 23 — DSP preview effects on a worker thread

The DSP group reserved in the Properties inspector now holds a real effect
selector: **None / Grayscale / Invert / Blur**, plus an intensity slider that
blends the processed frame back over the untouched one.

The effects are ported from the sibling learning project
`D:\Qt\Samples\ThreadedEffectPreview`. Its `EffectType` and `FrameProcessor`
become `ClipEffect.h` and `QtFrameEffectProcessor` here, keeping that sample's
two rules: the worker owns no widgets, and it checks
`isInterruptionRequested()` between image rows so shutdown never waits for a
frame to finish. Those effects are CPU work on a `QImage`; moving them onto the
GPU is a separate, later step.

### A real per-clip edit decision

DSP lives in `ClipSettings`, beside opacity, position, and fades. Each timeline
placement can therefore have a different effect and intensity. DSP edits use
the existing `clipSettingsEdited` path, mark the document dirty, participate in
Undo/Redo, and are stored in the v8 `.mini-editor.json` project file. Selecting
a source-library asset still shows the unmodified source because source
inspection is not a timeline placement edit.

Timeline thumbnails use a separate derived cache keyed by clip ID, source
thumbnail identity, effect, and intensity. A DSP edit regenerates only the
affected placement's small thumbnail during view refresh; `paintEvent()` only
reads the prepared image. Media Library continues to display the untouched
source thumbnail, and another placement of the same asset keeps its own result.

### Back-pressure: conflate, do not queue

This is the one place the original sample's design could not be copied.
`ThreadedEffectPreview` applies back-pressure by disabling its Process button
until a result returns. A live preview cannot do that — decoded frames keep
arriving regardless.

`QtPreviewEffectPipeline` therefore **conflates**: while the worker is busy it
remembers only the newest frame and drops the ones in between, so the work
queue can never grow without bound and the preview always catches up to the
current frame. `droppedFrameCount()` exposes that for testing. Painting prefers
the last processed frame but never waits for one, so an expensive effect
degrades the preview's update rate rather than freezing the UI thread.

When the effect is `None`, `submit()` returns false and the panel paints the
decoded frame directly, so an idle DSP section costs nothing — no copy, no
thread hop.

`clear()` also advances the accepted request generation. A result already in
progress may finish, but its old request ID is ignored, so changing clips,
sources, or effects can never paint a stale processed frame over the current
preview.

## Build with Visual Studio 2022

Open `D:\Qt\Samples\MiniEditorCoexistence` with **File > Open > Folder**.
Choose the `vs2022-x64` CMake preset, build `MiniEditorCoexistence`, and run
it.

The Visual Studio **Desktop development with C++** workload must include
**C++ MFC for latest v143 build tools**.

The Qt installation must include **Qt Multimedia** for the MSVC 2022 64-bit
kit. CMake links the Qt-enabled application with `Qt6::Multimedia` and
`Qt6::MultimediaWidgets`; `windeployqt` copies their FFmpeg/media plugins and
runtime libraries beside the executable.

## Headless Qt widget regression tests

`MiniEditorQtWidgetTests` uses Qt Test with `QT_QPA_PLATFORM=offscreen` to
exercise migrated panels without opening the MFC application. The tests cover
model-to-view refresh blocking, semantic Properties edits, Transport commands
and timecode, and Media Library selection/import/removal requests. Stable
widget object names make these controls testable without exposing private
implementation pointers.

The suite also drives the custom timeline directly: clip selection, empty-area
focus, ruler seeking, Delete, body dragging, end trimming, and the A1 visibility
button are verified through the same semantic callbacks used by `MainFrame`.

Build the `check` target to build the application and both test executables,
then run them through CTest. The test target deploys the Qt offscreen platform
plugin beside the executable automatically.
