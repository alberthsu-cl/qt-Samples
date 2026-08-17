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

## Headless regression test

`tests/tst_frameprocessor.cpp` is a C++ Qt Test executable. It starts a real
`QThread`, moves a `FrameProcessor` onto it, queues an image-processing request,
and waits for `processingFinished` with `QSignalSpy`. The test then verifies the
result pixels and confirms that the processing happened away from the test's
main thread.

The Python wrapper is deliberately small: it starts the C++ test as a separate
process through CTest, saves the output as a timestamped regression log, and
returns the same pass/fail exit code. Python controls the process boundary;
Qt controls the worker thread inside that process.

From a Developer PowerShell or a Visual Studio developer command prompt:

```powershell
cd D:\Qt\Samples\ThreadedEffectPreview
py tests\run_regression.py
```

Use `py tests\run_regression.py --configuration Release` for the Release build.
Logs are written below `test-results\`, which is ignored by Git.

## Build with Visual Studio 2022

Open `D:\Qt\Samples\ThreadedEffectPreview` with **File > Open > Folder** in Visual Studio 2022. Select the `vs2022-x64` preset and build `ThreadedEffectPreview`.

The post-build CMake command runs `windeployqt`, so the executable can also be launched directly from its Debug/Release output folder.
