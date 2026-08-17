#pragma once

#include "EffectType.h"

#include <QObject>
#include <QImage>

// This object deliberately owns no widgets. After moveToThread(), its slots
// execute in the worker thread whenever the UI emits a queued signal.
class FrameProcessor final : public QObject
{
    Q_OBJECT

public slots:
    void processImage(int requestId, const QImage &image, EffectType effect);

signals:
    // This signal is delivered back to PreviewWindow's UI thread.
    void processingFinished(int requestId,
                            const QImage &result,
                            const QString &workerThreadId);
};
