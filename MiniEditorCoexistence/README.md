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

`EditorPaneBase` contains only shared MFC painting mechanics. `MediaLibraryPane`,
`PreviewPane`, `PropertiesPane`, and `TimelinePane` each own their own drawing
and input behavior. This gives the migration a one-to-one seam: the Phase 1
Qt Media Library can replace `MediaLibraryPane` without turning a large
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

Set `MINI_EDITOR_USE_QT_MEDIA_LIBRARY` to `OFF` in the CMake cache to rebuild
the exact Phase 0 pure-MFC Media Library for comparison.

## Build with Visual Studio 2022

Open `D:\Qt\Samples\MiniEditorCoexistence` with **File > Open > Folder**.
Choose the `vs2022-x64` CMake preset, build `MiniEditorCoexistence`, and run
it.

The Visual Studio **Desktop development with C++** workload must include
**C++ MFC for latest v143 build tools**.
