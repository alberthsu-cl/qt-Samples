#pragma once

// A bitmask describing exactly which part of editor state changed. Keeping
// this framework-neutral lets MFC and Qt refresh only the views they own.
enum class EditorChange : unsigned int {
    None = 0,
    Selection = 1 << 0,
    ClipSettings = 1 << 1,
    Playback = 1 << 2,
    TimelineView = 1 << 3,
    TimelineClip = 1 << 4,
    All = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4)
};

constexpr EditorChange operator|(EditorChange left, EditorChange right)
{
    return static_cast<EditorChange>(static_cast<unsigned int>(left)
        | static_cast<unsigned int>(right));
}

constexpr bool includesChange(EditorChange changes, EditorChange requestedChange)
{
    return (static_cast<unsigned int>(changes)
        & static_cast<unsigned int>(requestedChange)) != 0;
}
