#include "EditorProject.h"

EditorProject EditorProject::createDefault(std::size_t assetCount)
{
    EditorProject project;
    project.clipSettings.resize(assetCount);
    project.timelineClips.resize(assetCount);
    return project;
}
