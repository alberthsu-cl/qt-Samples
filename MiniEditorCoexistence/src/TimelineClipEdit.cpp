#include "TimelineClipEdit.h"

#include <algorithm>

TimelineClipState TimelineClipEdit::moveTo(const TimelineClipState &original,
                                           int startFrame)
{
    TimelineClipState edited = original;
    edited.startFrame = std::max(0, startFrame);
    return edited;
}

TimelineClipState TimelineClipEdit::trimStartTo(const TimelineClipState &original,
                                                int startFrame,
                                                const TimelineTrimContext &context)
{
    const int originalEnd = original.startFrame + std::max(1, original.durationFrames);
    if (context.mediaKind == MediaKind::Image) {
        const int editedStart = std::clamp(startFrame, 0, originalEnd - 1);
        return { editedStart, originalEnd - editedStart, 0 };
    }

    const int earliestStart = std::max(0, original.startFrame - original.sourceInFrame);
    const int editedStart = std::clamp(startFrame, earliestStart, originalEnd - 1);
    const int trimDelta = editedStart - original.startFrame;
    return { editedStart, originalEnd - editedStart,
             original.sourceInFrame + trimDelta };
}

TimelineClipState TimelineClipEdit::trimEndTo(const TimelineClipState &original,
                                              int endFrame,
                                              const TimelineTrimContext &context)
{
    if (context.mediaKind == MediaKind::Image) {
        const int editedEnd = std::max(original.startFrame + 1, endFrame);
        return { original.startFrame, editedEnd - original.startFrame, 0 };
    }

    const int availableSourceFrames = std::max(
        1, context.sourceDurationFrames - original.sourceInFrame);
    const int latestEnd = original.startFrame + availableSourceFrames;
    const int editedEnd = std::clamp(endFrame, original.startFrame + 1,
                                     latestEnd);
    return { original.startFrame, editedEnd - original.startFrame,
             original.sourceInFrame };
}
