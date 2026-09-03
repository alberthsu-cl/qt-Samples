#pragma once

#include "EditorProject.h"
#include "playback_core/ProjectRuntime.h"
#include "playback_core/SequencePlaybackSnapshot.h"

// Editor-thread transaction boundary. It reads completed editor values and
// returns either one self-contained immutable snapshot or one explicit error.
// The published snapshot never references these mutable input containers.
class SequencePlaybackSnapshotBuilder final {
public:
    static mini_editor::playback_core::SnapshotBuildResult build(
        const EditorProject &project,
        const mini_editor::playback_core::ProjectRuntime &runtime);
};
