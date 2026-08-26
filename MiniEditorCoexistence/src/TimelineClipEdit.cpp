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
                                                int startFrame)
{
    const int originalEnd = original.startFrame + std::max(1, original.durationFrames);
    const int editedStart = std::clamp(startFrame, original.startFrame,
                                       originalEnd - 1);
    return { editedStart, originalEnd - editedStart };
}

TimelineClipState TimelineClipEdit::trimEndTo(const TimelineClipState &original,
                                              int endFrame)
{
    const int originalEnd = original.startFrame + std::max(1, original.durationFrames);
    const int editedEnd = std::clamp(endFrame, original.startFrame + 1,
                                     originalEnd);
    return { original.startFrame, editedEnd - original.startFrame };
}
