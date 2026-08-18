#pragma once

#include "EffectSettings.h"

#include <QImage>
#include <QMetaType>
#include <QObject>
#include <QThread>

#include <functional>

Q_DECLARE_METATYPE(EffectType)

// The worker owns no widgets. Its slots run on workerThread_, while the
// controller itself remains on the MFC/Qt UI thread.
class QtImageEffectWorker final : public QObject
{
    Q_OBJECT

public slots:
    void process(QImage sourceImage, EffectType effect, quint64 requestId);

signals:
    void processingFinished(QImage processedImage, quint64 requestId);
};

// This controller is the safe bridge from MFC-owned MainFrame to a Qt worker.
// Its completion callback is invoked on the UI thread, never on workerThread_.
class QtAsyncEffectProcessor final : public QObject
{
public:
    using CompletionHandler = std::function<void(QImage processedImage,
                                                 quint64 requestId)>;

    QtAsyncEffectProcessor();
    ~QtAsyncEffectProcessor() override;

    QtAsyncEffectProcessor(const QtAsyncEffectProcessor &) = delete;
    QtAsyncEffectProcessor &operator=(const QtAsyncEffectProcessor &) = delete;

    void requestProcessing(const QImage &sourceImage,
                           EffectType effect,
                           quint64 requestId);
    void setCompletionHandler(CompletionHandler handler);

private:
    QThread workerThread_;
    QtImageEffectWorker *worker_ = nullptr;
    CompletionHandler completionHandler_;
};
