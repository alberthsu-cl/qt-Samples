#pragma once

#include "AudioWaveform.h"
#include "AudioWaveformDisplayCache.h"
#include "TimelineModel.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QThread>

#include <cstdint>
#include <vector>

class MediaLibrary;

// Owns decoded PCM peak data for real audio assets. Decoding happens on a
// dedicated Qt thread; timeline painting only asks for pixel-ready columns.
class QtAudioWaveformCache final : public QObject
{
    Q_OBJECT

public:
    explicit QtAudioWaveformCache(QObject *parent = nullptr);
    ~QtAudioWaveformCache() override;

    void refresh(const MediaLibrary &mediaLibrary);
    void requestForTimeline(int mediaAssetId);
    SharedAudioWaveform waveformForClip(
        const TimelineClip &clip, int pixelWidth,
        int timelineFramesPerSecond = 30);

signals:
    void decodeRequested(int mediaAssetId, QString filePath,
                         std::uint64_t generation);
    void waveformChanged(int mediaAssetId);

private:
    QThread workerThread_;
    QHash<int, QString> audioFilePaths_;
    QHash<int, std::uint64_t> generations_;
    QHash<int, AudioWaveformData> waveforms_;
    QSet<int> pendingAssetIds_;
    AudioWaveformDisplayCache displayCache_;
    std::uint64_t nextGeneration_ = 1;
};
