#pragma once

#include "TimelineModel.h"

#include <QColor>
#include <QString>

// Everything the timeline needs to preview a library asset before it has
// become a real TimelineClip in the project model.
struct TimelineAssetPresentation {
    QString displayName;
    QColor color;
    TimelineTrackType trackType = TimelineTrackType::Video;
    int durationFrames = 0;
};
