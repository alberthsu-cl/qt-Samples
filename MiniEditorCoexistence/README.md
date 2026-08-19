# Mini Editor Coexistence

This sample models the **UI structure** of a desktop video editor while keeping
the media engine out of scope. It is a learning project for gradual MFC-to-Qt
migration, not a video editor implementation.

## Phase 0 — Pure MFC baseline

The application contains four custom MFC child windows:

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

## Build with Visual Studio 2022

Open `D:\Qt\Samples\MiniEditorCoexistence` with **File > Open > Folder**.
Choose the `vs2022-x64` CMake preset, build `MiniEditorCoexistence`, and run
it.

The Visual Studio **Desktop development with C++** workload must include
**C++ MFC for latest v143 build tools**.
