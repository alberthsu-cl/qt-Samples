#pragma once

#include "ClipEffect.h"

#include <QImage>
#include <QObject>
#include <QThread>

class QtFrameEffectProcessor;

// Owns the effect worker thread for the preview.
//
// ThreadedEffectPreview applies back-pressure by disabling its Process button
// until a result returns. A live preview cannot do that: frames keep arriving
// whether or not the previous one finished. This pipeline therefore conflates
// instead of queueing - while the worker is busy it remembers only the newest
// frame and discards the ones in between. The preview stays current and the
// work queue can never grow without bound.
class QtPreviewEffectPipeline final : public QObject
{
    Q_OBJECT

public:
    explicit QtPreviewEffectPipeline(QObject *parent = nullptr);
    ~QtPreviewEffectPipeline() override;

    QtPreviewEffectPipeline(const QtPreviewEffectPipeline &) = delete;
    QtPreviewEffectPipeline &operator=(const QtPreviewEffectPipeline &) = delete;

    // Requests processing of one frame. Returns false when the effect is a
    // no-op, in which case the caller should paint the frame directly instead
    // of paying for a thread hop.
    bool submit(const QImage &frame, ClipEffectKind effect, int intensityPercent);
    void clear();

    // Frames accepted but never processed because a newer frame replaced them.
    int droppedFrameCount() const;

signals:
    void frameProcessed(const QImage &result);

private:
    void dispatchPendingFrame();

    QThread workerThread_;
    QtFrameEffectProcessor *processor_ = nullptr;
    bool isWorkerBusy_ = false;
    bool hasPendingFrame_ = false;
    QImage pendingFrame_;
    ClipEffectKind pendingEffect_ = ClipEffectKind::None;
    int pendingIntensityPercent_ = 100;
    int nextRequestId_ = 1;
    int activeRequestId_ = 0;
    int minimumAcceptedRequestId_ = 1;
    int droppedFrameCount_ = 0;
};
