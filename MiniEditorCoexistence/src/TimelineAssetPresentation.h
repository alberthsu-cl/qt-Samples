#pragma once

#include "MediaKind.h"
#include "TimelineModel.h"

#include <QColor>
#include <QImage>
#include <QString>

// Everything the timeline needs to preview a library asset before it has
// become a real TimelineClip in the project model.
struct TimelineAssetPresentation {
    QString displayName;
    QColor color;
    TimelineTrackType trackType = TimelineTrackType::Video;
    MediaKind mediaKind = MediaKind::Video;
    int durationFrames = 0;
    QImage thumbnail;
    // Imported files already identify themselves through their real
    // thumbnails. Built-in sample cards still need their text labels.
    bool isRealAsset = false;
};
