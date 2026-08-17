# Threaded Effect Preview

A small Qt Widgets project for learning safe communication between the UI thread and a worker thread.

## What it demonstrates

```text
PreviewWindow (UI thread)
    │ emits processRequested(requestId, copied QImage, effect)
    ▼
FrameProcessor (worker QThread)
    │ applies a CPU effect; never touches a widget
    ▼
PreviewWindow (UI thread)
    updates the result QLabel after processingFinished(...)
```

The source image and processed result appear side by side. A generated image is shown at startup, so no media file is required.

## Key files

- `PreviewWindow.*` — UI setup, signal emission, and result display. All methods that touch widgets run on the main/UI thread.
- `FrameProcessor.*` — worker object. It is moved to `QThread` with `moveToThread()` and only processes `QImage` data.
- `EffectType.h` — strongly typed effect value that crosses the thread boundary.

## Important Qt threading rules

1. Create widgets and access widgets only on the UI thread.
2. Move a `QObject` worker to a `QThread`; do not put processing code inside a subclass of `QThread` for this first example.
3. Connect UI signals to worker slots. Qt queues calls automatically when objects have different thread affinity.
4. Send `sourceImage_.copy()` to the worker, so it owns an independent image snapshot.
5. Disable the Process button until a result returns. This is a small form of back-pressure: the sample does not build an unlimited work queue.
6. During application shutdown, call `requestInterruption()`, `quit()`, and `wait()` on the worker thread. Interruption is cooperative, so the worker checks the flag while processing rows of an image.

## Build with Visual Studio 2022

Open `D:\Qt\Samples\ThreadedEffectPreview` with **File > Open > Folder** in Visual Studio 2022. Select the `vs2022-x64` preset and build `ThreadedEffectPreview`.

The post-build CMake command runs `windeployqt`, so the executable can also be launched directly from its Debug/Release output folder.
