# MFC / Qt Coexistence — Phase 1

This is a gradual MFC-to-Qt UI migration exercise.

- **Phase 1** created the original MFC-only baseline.
- **Phase 2 (current)** keeps the MFC main frame, image display, and image
  processing, but replaces only the settings dialog with a Qt `QDialog`.

## Behavior

1. Start the MFC main window. A generated color image is displayed by default.
2. Optionally select **File > Open Image...** to load a PNG, JPEG, or BMP.
3. Select **Effects > Settings...**, choose `No effect`, `Grayscale`,
   `Invert`, or `Blur`, and click **OK**.
4. The status bar shows the selected effect and the display updates.
5. Click the image-only comparison button to compare both images in the same
   display area. A checked icon means the applied effect is displayed; an
   unchecked icon means the original is displayed.
6. Closing **Effects > Settings...** always returns to the applied-result view.

## Why this shape?

`EffectSettings.h` is deliberately independent of MFC and Qt. It is the
boundary we preserve in Phase 2:

```text
Phase 1: MFC menu → MFC dialog → EffectSettings
Phase 2: MFC menu → Qt dialog  → EffectSettings
```

Only the dialog/presentation layer changes in Phase 2. The MFC main window and
the shared settings model remain stable. `QtRuntime.*` creates the single
`QApplication` required for Qt Widgets in the MFC process.

`ImageProcessor.*` applies the CPU effects and `ImageDisplayWindow.*` owns the
aspect-ratio-preserving display. `MainFrame` reserves space for its standard
MFC status bar before arranging the image display and comparison button. These
are also useful seams for later Qt replacement work.

## Build with Visual Studio 2022

Open `D:\Qt\Samples\MfcQtCoexistence` with **File > Open > Folder**. Select
the `vs2022-x64` CMake preset, build `MfcQtCoexistence`, then run it.

The Visual Studio **Desktop development with C++** workload must include the
**C++ MFC for latest v143 build tools** component.

The project is intentionally compiled in Unicode mode (`UNICODE` and
`_UNICODE`). This keeps Windows/MFC text handling consistent with the UTF-16
strings used by the sample.

## Compare the two dialog implementations

By default, CMake uses the Qt dialog:

```text
MFCQT_USE_QT_SETTINGS_DIALOG = ON
```

Set it to `OFF` in Visual Studio's CMake cache variables (then reconfigure) to
run the original `EffectSettingsDialog` MFC version again. Both dialogs edit
the same `EffectSettings` data model and therefore produce identical image
effects.
