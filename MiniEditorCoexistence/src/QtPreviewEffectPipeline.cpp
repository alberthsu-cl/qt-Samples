#include "QtPreviewEffectPipeline.h"

#include "QtFrameEffectProcessor.h"

#include <algorithm>

QtPreviewEffectPipeline::QtPreviewEffectPipeline(QObject *parent)
    : QObject(parent)
    , processor_(new QtFrameEffectProcessor)
{
    qRegisterMetaType<ClipEffectKind>("ClipEffectKind");

    processor_->moveToThread(&workerThread_);
    // The worker is owned by its thread's finished signal, which is the
    // standard way to guarantee it is destroyed on the thread that ran it.
    connect(&workerThread_, &QThread::finished,
            processor_, &QObject::deleteLater);
    connect(processor_, &QtFrameEffectProcessor::frameProcessed, this,
            [this](int requestId, const QImage &result) {
                isWorkerBusy_ = false;
                const bool isStillCurrent = requestId == activeRequestId_
                    && requestId >= minimumAcceptedRequestId_;
                if (isStillCurrent && !result.isNull())
                    emit frameProcessed(result);
                // A frame that arrived while the worker was busy waited for
                // this moment rather than joining an unbounded queue.
                dispatchPendingFrame();
            });
    workerThread_.start();
}

QtPreviewEffectPipeline::~QtPreviewEffectPipeline()
{
    // Interruption is cooperative: the worker checks the flag between image
    // rows, so a frame in progress abandons its work instead of delaying exit.
    workerThread_.requestInterruption();
    workerThread_.quit();
    workerThread_.wait();
}

bool QtPreviewEffectPipeline::submit(const QImage &frame, ClipEffectKind effect,
                                     int intensityPercent)
{
    const int intensity = std::clamp(intensityPercent,
                                     kMinimumEffectIntensityPercent,
                                     kMaximumEffectIntensityPercent);
    if (effect == ClipEffectKind::None || intensity == 0 || frame.isNull()) {
        clear();
        return false;
    }

    if (hasPendingFrame_)
        ++droppedFrameCount_;

    // copy() detaches from the decoder's buffer, so the worker owns an
    // independent snapshot exactly as ThreadedEffectPreview requires.
    pendingFrame_ = frame.copy();
    pendingEffect_ = effect;
    pendingIntensityPercent_ = intensity;
    hasPendingFrame_ = true;
    dispatchPendingFrame();
    return true;
}

void QtPreviewEffectPipeline::clear()
{
    hasPendingFrame_ = false;
    pendingFrame_ = QImage();
    // A running request cannot be removed from the worker event loop, but its
    // ID can be made obsolete. This prevents an old source/effect result from
    // appearing after the UI has already moved on.
    minimumAcceptedRequestId_ = nextRequestId_;
}

int QtPreviewEffectPipeline::droppedFrameCount() const
{
    return droppedFrameCount_;
}

void QtPreviewEffectPipeline::dispatchPendingFrame()
{
    if (isWorkerBusy_ || !hasPendingFrame_)
        return;

    const QImage frame = pendingFrame_;
    const ClipEffectKind effect = pendingEffect_;
    const int intensity = pendingIntensityPercent_;
    hasPendingFrame_ = false;
    pendingFrame_ = QImage();
    isWorkerBusy_ = true;
    activeRequestId_ = nextRequestId_++;

    // A queued invocation hands the frame to the worker thread. The UI thread
    // returns immediately and keeps painting the previous result.
    QMetaObject::invokeMethod(processor_, "processFrame", Qt::QueuedConnection,
                              Q_ARG(int, activeRequestId_),
                              Q_ARG(QImage, frame),
                              Q_ARG(ClipEffectKind, effect),
                              Q_ARG(int, intensity));
}
