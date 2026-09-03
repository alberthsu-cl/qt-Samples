#include "ProjectRuntime.h"

#include <atomic>
#include <utility>

namespace mini_editor::playback_core {
namespace {

std::atomic<std::uint64_t> nextProjectId { 1 };
std::atomic<std::uint64_t> nextSequenceId { 1 };

} // namespace

ProjectId::ProjectId(std::uint64_t value) : value_(value) {}
ProjectId ProjectId::create()
{
    return ProjectId(nextProjectId.fetch_add(1));
}
std::uint64_t ProjectId::valueForDiagnostics() const { return value_; }
bool operator==(ProjectId left, ProjectId right)
{
    return left.valueForDiagnostics() == right.valueForDiagnostics();
}
bool operator!=(ProjectId left, ProjectId right) { return !(left == right); }

SequenceId::SequenceId(std::uint64_t value) : value_(value) {}
SequenceId SequenceId::create()
{
    return SequenceId(nextSequenceId.fetch_add(1));
}
std::uint64_t SequenceId::valueForDiagnostics() const { return value_; }
bool operator==(SequenceId left, SequenceId right)
{
    return left.valueForDiagnostics() == right.valueForDiagnostics();
}
bool operator!=(SequenceId left, SequenceId right) { return !(left == right); }

SequenceRevision::SequenceRevision(std::uint64_t value) : value_(value) {}
SequenceRevision SequenceRevision::initial() { return SequenceRevision(0); }
std::uint64_t SequenceRevision::value() const { return value_; }
SequenceRevision SequenceRevision::next() const { return SequenceRevision(value_ + 1); }
bool operator==(SequenceRevision left, SequenceRevision right)
{
    return left.value() == right.value();
}
bool operator!=(SequenceRevision left, SequenceRevision right) { return !(left == right); }
bool operator<(SequenceRevision left, SequenceRevision right)
{
    return left.value() < right.value();
}

ProjectRuntime::ProjectRuntime(ProjectId projectId,
                               std::vector<TimelineSequence> sequences,
                               std::optional<SequenceId> activeSequenceId,
                               ProjectReadiness readiness,
                               std::optional<ProjectError> error)
    : projectId_(projectId)
    , sequences_(std::move(sequences))
    , activeSequenceId_(activeSequenceId)
    , readiness_(readiness)
    , error_(std::move(error))
{
}

ProjectRuntime ProjectRuntime::loading()
{
    return ProjectRuntime(ProjectId::create(), {}, std::nullopt,
                          ProjectReadiness::Loading, std::nullopt);
}

ProjectRuntime ProjectRuntime::fromLegacyFlatProject(std::size_t timelineClipCount)
{
    TimelineSequence sequence {
        SequenceId::create(),
        "Primary sequence",
        FrameRate(30, 1),
        SequenceRevision::initial(),
        timelineClipCount
    };
    const SequenceId activeSequenceId = sequence.id;
    const ProjectReadiness readiness = timelineClipCount == 0
        ? ProjectReadiness::Empty : ProjectReadiness::Ready;

    return ProjectRuntime(ProjectId::create(), { std::move(sequence) },
                          activeSequenceId, readiness, std::nullopt);
}

ProjectRuntime ProjectRuntime::failed(std::string message)
{
    return ProjectRuntime(ProjectId::create(), {}, std::nullopt,
                          ProjectReadiness::Failed,
                          ProjectError { std::move(message) });
}

const ProjectId &ProjectRuntime::projectId() const { return projectId_; }
const std::vector<TimelineSequence> &ProjectRuntime::sequences() const
{
    return sequences_;
}
std::optional<SequenceId> ProjectRuntime::activeSequenceId() const
{
    return activeSequenceId_;
}
ProjectReadiness ProjectRuntime::readiness() const { return readiness_; }
const std::optional<ProjectError> &ProjectRuntime::error() const { return error_; }

void ProjectRuntime::setLegacySequenceClipCount(std::size_t timelineClipCount)
{
    if (!activeSequenceId_ || sequences_.size() != 1)
        return;

    sequences_.front().timelineClipCount = timelineClipCount;
    updateReadinessFromActiveSequence();
}

void ProjectRuntime::advanceActiveSequenceRevision()
{
    if (!activeSequenceId_ || sequences_.size() != 1)
        return;

    sequences_.front().revision = sequences_.front().revision.next();
}

void ProjectRuntime::updateReadinessFromActiveSequence()
{
    if (readiness_ == ProjectReadiness::Failed)
        return;

    if (!activeSequenceId_ || sequences_.empty()) {
        readiness_ = ProjectReadiness::Empty;
        return;
    }

    readiness_ = sequences_.front().timelineClipCount == 0
        ? ProjectReadiness::Empty : ProjectReadiness::Ready;
}

} // namespace mini_editor::playback_core
