# Mini Editor Coexistence

This sample models the **UI structure** of a desktop video editor while keeping
the media engine out of scope. It is a learning project for gradual MFC-to-Qt
migration, not a video editor implementation.

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

The Qt-enabled target contains only MFC files that it still uses:
`MfcPreviewCanvas`, `MfcTimelineCanvas`, `MfcWorkspaceSplitter`, and their
shared `MfcEditorPaneBase`. The complete fallback panes are included only in
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
Qt Timeline toolbar (zoom, fit, audio visibility)
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

`EditorSession` records only `ClipSettings` edits (opacity, scale, and
position) in undo/redo history. Playback, selection, timeline view, and
workspace layout are deliberately excluded because they are transient UI
state, not project-edit decisions. The MFC Edit menu invokes the same
framework-neutral `undo()` and `redo()` operations, and enables each action
only when the matching history stack has an entry.

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

## Phase 12 — Flicker-free MFC playback painting

`MfcDoubleBufferedPaint` draws MFC preview and timeline content to a
compatible off-screen bitmap, then copies the completed frame to the window
with one `BitBlt`. Playback invalidation uses `Invalidate(FALSE)`, and the
paint handlers suppress the redundant `WM_ERASEBKGND` pass. This prevents the
background from becoming visible between frequent playback repaints.

## Build with Visual Studio 2022

Open `D:\Qt\Samples\MiniEditorCoexistence` with **File > Open > Folder**.
Choose the `vs2022-x64` CMake preset, build `MiniEditorCoexistence`, and run
it.

The Visual Studio **Desktop development with C++** workload must include
**C++ MFC for latest v143 build tools**.
