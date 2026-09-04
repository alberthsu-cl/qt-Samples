# Qt Samples

Qt 6 and Visual Studio 2022 learning projects, from a simple player to a
gradual MFC-to-Qt video-editor migration.

## Projects

| Project | What it demonstrates |
| --- | --- |
| [`SimpleVideoPlayer`](SimpleVideoPlayer/README.md) | Qt Widgets video playback with `QMediaPlayer`/`QVideoSink`, a Direct3D 11 texture path, and HLSL effects. |
| [`ThreadedEffectPreview`](ThreadedEffectPreview/README.md) | A `QObject` worker on a `QThread`, queued communication, copied `QImage` work, and cooperative cancellation. |
| [`MfcQtCoexistence`](MfcQtCoexistence/README.md) | A focused MFC-to-Qt coexistence migration: MFC frame shell with progressively replaced Qt panels and asynchronous effects. |
| [`MiniEditorCoexistence`](MiniEditorCoexistence/README.md) | The main video-editor learning sample: media library, timeline editing, real Qt multimedia preview, strong playback-core contracts, and a staged MFC-to-Qt migration. |

## Mini Editor: current architecture study

`MiniEditorCoexistence` has completed Playback-engine **Milestones 1–4**:

- Framework-neutral playback core with strong time and identity types.
- Immutable sequence snapshots, a serialized engine thread, bounded video-work scheduling, and presentation identities.
- Real Qt Multimedia adapter, MFC `PostMessage` notification bridge, validated failure routing, and a feature-flagged timeline-engine route.

The new route remains **off by default** while Milestone 5 compares it with
the legacy implementation and retires legacy timer advancement deliberately.
Read its [architecture decisions](MiniEditorCoexistence/docs/architecture/decisions/README.md), [milestone roadmap](MiniEditorCoexistence/docs/architecture/milestones/README.md), and [learning journal](MiniEditorCoexistence/docs/Timeline-Playback-Engine-Learning.md).

Each sample has its own README, `CMakeLists.txt`, and Visual Studio 2022 CMake preset.
