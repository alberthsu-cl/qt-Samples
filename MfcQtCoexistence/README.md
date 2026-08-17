# MFC / Qt Coexistence — Phase 1

This is the **pure MFC baseline** for a gradual UI migration exercise. It has
no Qt headers, libraries, widgets, or Qt event loop.

## Behavior

1. Start the MFC main window.
2. Select **Effects > Settings...**.
3. Choose `No effect`, `Grayscale`, `Invert`, or `Blur`.
4. Click **OK**. The status bar shows the selected effect.

## Why this shape?

`EffectSettings.h` is deliberately independent of MFC and Qt. It is the
boundary we preserve in Phase 2:

```text
Phase 1: MFC menu → MFC dialog → EffectSettings
Phase 2: MFC menu → Qt dialog  → EffectSettings
```

Only the dialog/presentation layer changes in Phase 2. The MFC main window and
the shared settings model remain stable.

## Build with Visual Studio 2022

Open `D:\Qt\Samples\MfcQtCoexistence` with **File > Open > Folder**. Select
the `vs2022-x64` CMake preset, build `MfcQtCoexistence`, then run it.

The Visual Studio **Desktop development with C++** workload must include the
**C++ MFC for latest v143 build tools** component.
