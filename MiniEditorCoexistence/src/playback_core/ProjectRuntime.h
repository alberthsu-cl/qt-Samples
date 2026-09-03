#pragma once

#include "MediaTime.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mini_editor::playback_core {

class ProjectId final {
public:
    static ProjectId create();

    std::uint64_t valueForDiagnostics() const;

private:
    explicit ProjectId(std::uint64_t value);

    std::uint64_t value_;
};

class SequenceId final {
public:
    static SequenceId create();

    std::uint64_t valueForDiagnostics() const;

private:
    explicit SequenceId(std::uint64_t value);

    std::uint64_t value_;
};

class SequenceRevision final {
public:
    static SequenceRevision initial();

    std::uint64_t value() const;
    SequenceRevision next() const;

private:
    explicit SequenceRevision(std::uint64_t value);

    std::uint64_t value_;
};

bool operator==(ProjectId left, ProjectId right);
bool operator!=(ProjectId left, ProjectId right);
bool operator==(SequenceId left, SequenceId right);
bool operator!=(SequenceId left, SequenceId right);
bool operator==(SequenceRevision left, SequenceRevision right);
bool operator!=(SequenceRevision left, SequenceRevision right);
bool operator<(SequenceRevision left, SequenceRevision right);

enum class ProjectReadiness {
    Loading,
    Ready,
    Empty,
    Failed
};

struct ProjectError final {
    std::string message;
};

// The runtime representation of one sequence. Its identity is deliberately
// not serialized by the legacy flat project format: reloading identical file
// contents must still receive a new SequenceId.
struct TimelineSequence final {
    SequenceId id;
    std::string name;
    FrameRate frameRate;
    SequenceRevision revision;
    std::size_t timelineClipCount = 0;
};

// Framework-neutral state for one loaded project. This is intentionally
// separate from EditorProject, whose job is to represent persisted file data.
class ProjectRuntime final {
public:
    static ProjectRuntime loading();
    static ProjectRuntime fromLegacyFlatProject(std::size_t timelineClipCount);
    static ProjectRuntime failed(std::string message);

    const ProjectId &projectId() const;
    const std::vector<TimelineSequence> &sequences() const;
    std::optional<SequenceId> activeSequenceId() const;
    ProjectReadiness readiness() const;
    const std::optional<ProjectError> &error() const;

    // Legacy Mini Editor has one sequence. Updating its clip count preserves
    // that sequence identity while changing only its readiness state.
    void setLegacySequenceClipCount(std::size_t timelineClipCount);
    void advanceActiveSequenceRevision();

private:
    ProjectRuntime(ProjectId projectId, std::vector<TimelineSequence> sequences,
                   std::optional<SequenceId> activeSequenceId,
                   ProjectReadiness readiness,
                   std::optional<ProjectError> error);

    void updateReadinessFromActiveSequence();

    ProjectId projectId_;
    std::vector<TimelineSequence> sequences_;
    std::optional<SequenceId> activeSequenceId_;
    ProjectReadiness readiness_;
    std::optional<ProjectError> error_;
};

} // namespace mini_editor::playback_core
