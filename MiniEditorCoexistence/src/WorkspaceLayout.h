#pragma once

// Framework-neutral persisted dimensions for the editor workspace.
struct WorkspaceLayoutState {
    int mediaLibraryWidth = 304;
    int propertiesWidth = 250;
    int timelineHeight = 220;
};

// Framework-neutral child bounds in the MFC frame's client coordinates.
struct WorkspaceRect {
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
};

struct WorkspaceGeometry {
    WorkspaceRect mediaLibrary;
    WorkspaceRect previewCanvas;
    WorkspaceRect transport;
    WorkspaceRect properties;
    WorkspaceRect timeline;
    WorkspaceRect timelineToolbar;
    WorkspaceRect timelineCanvas;
    WorkspaceRect leftSplitter;
    WorkspaceRect rightSplitter;
    WorkspaceRect timelineSplitter;
};

// Owns layout policy only: defaults, minimum sizes, splitter changes, and
// rectangle calculation. It deliberately does not include MFC or Qt headers.
class WorkspaceLayout final
{
public:
    WorkspaceLayoutState state() const;
    void setState(const WorkspaceLayoutState &state);

    WorkspaceGeometry calculate(int clientWidth, int contentBottom);

    void moveLeftSplitter(int parentX);
    void moveRightSplitter(int parentX, int clientWidth);
    void moveTimelineSplitter(int parentY, int contentBottom);

private:
    WorkspaceLayoutState state_;
};
