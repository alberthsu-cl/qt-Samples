# QML Mini Editor

A small, clean Qt Quick / QML learning project for a video-editor-style UI.
It deliberately uses **fake media assets** and no decoder. The purpose of the
first lesson is to learn the QML/C++ boundary without mixing in playback,
threading, or GPU-video complexity.

## What Lesson 1 demonstrates

```text
C++ MediaLibraryModel + EditorViewModel
              │ Q_PROPERTY, roles, signal
              ▼
QML ApplicationWindow
  Media Library | Preview | Properties
  ───────────────────────────────────
  Timeline
```

- `MediaLibraryModel` is a C++ `QAbstractListModel`; its named roles become
  `title`, `kind`, `duration`, and `accentColor` in the QML delegate.
- `EditorViewModel` owns selection. Its `selectedAssetTitle` is a
  `Q_PROPERTY`, so Preview and Properties update automatically whenever QML
  calls `selectAsset(index)`.
- Both classes are intentionally not `final`: a type marked `QML_ELEMENT` is
  wrapped by Qt when QML creates it.
- `Main.qml` contains only layout and presentation. It never owns project,
  playback, or media-engine policy.

## Build and run from PowerShell

Use your installed Qt version in `CMAKE_PREFIX_PATH`. The commands below use
the current MSVC 2022 64-bit Qt kit.

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
  -S "D:\Qt\Samples\QmlMiniEditor" `
  -B "D:\Qt\Samples\QmlMiniEditor\out\build\vs2022-x64" `
  -G "Visual Studio 17 2022" -A x64 `
  "-DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64"

& "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
  --build "D:\Qt\Samples\QmlMiniEditor\out\build\vs2022-x64" `
  --target QmlMiniEditor --config Debug

& "D:\Qt\Samples\QmlMiniEditor\out\build\vs2022-x64\Debug\QmlMiniEditor.exe"
```

Or open `D:\Qt\Samples\QmlMiniEditor` through **Visual Studio 2022 → File →
Open → Folder**. In the configuration drop-down, select **Visual Studio 2022
x64 (Qt 6.11.2)**. The included `CMakePresets.json` supplies
`CMAKE_PREFIX_PATH`, so CMake can find Qt. Build `QmlMiniEditor`, then press
F5.

## Read this first

1. [`qml/Main.qml`](qml/Main.qml): QML layout, bindings, a `ListView`, and a
   delegate. Click a media card and observe every use of
   `editor.selectedAssetTitle` update without manual repaint calls.
2. [`src/EditorViewModel.h`](src/EditorViewModel.h): `Q_PROPERTY`,
   `Q_INVOKABLE`, and a change signal.
3. [`src/MediaLibraryModel.h`](src/MediaLibraryModel.h): a C++ model and the
   named role contract that QML consumes.
4. [`CMakeLists.txt`](CMakeLists.txt): `qt_add_qml_module`, which embeds QML
   and registers C++ types marked `QML_ELEMENT`.

## Suggested next lessons

1. Add a QML toolbar button that calls a C++ `Q_INVOKABLE` command.
2. Replace fake media cards with an importable C++ model.
3. Add a QML timeline model with selection and drag/drop.
4. Build a custom C++ `QQuickItem` for a performant timeline canvas.
5. Connect the proven playback-core contracts from `MiniEditorCoexistence`.
