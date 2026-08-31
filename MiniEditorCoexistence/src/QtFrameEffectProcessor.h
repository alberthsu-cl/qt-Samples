#pragma once

#include "ClipEffect.h"

#include <QImage>
#include <QObject>

Q_DECLARE_METATYPE(ClipEffectKind)

// Ported from ThreadedEffectPreview's FrameProcessor. This object owns no
// widgets: after moveToThread() its slot runs on the worker thread whenever
// the preview emits a queued request.
//
// It keeps that sample's cooperative-interruption contract, so a long frame
// abandons its work promptly when the application is shutting down.
class QtFrameEffectProcessor final : public QObject
{
    Q_OBJECT

public:
    // Applying an effect to one frame, without any thread involved. The
    // pipeline test and the worker slot share this, so what runs on the worker
    // thread is exactly what a direct call produces.
    static QImage applyEffect(const QImage &source, ClipEffectKind effect,
                              int intensityPercent, bool *wasInterrupted = nullptr);

public slots:
    void processFrame(int requestId, const QImage &frame, ClipEffectKind effect,
                      int intensityPercent);

signals:
    void frameProcessed(int requestId, const QImage &result);
};
