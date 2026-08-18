#include "QtAsyncEffectProcessor.h"

#include <QMetaObject>
#include <QMetaType>

#include <algorithm>
#include <utility>

namespace {

// A 7 x 7 box is intentionally strong enough to be obvious in this learning
// sample. It also gives the worker thread meaningful work on a large image.
constexpr int kBlurRadius = 3;
constexpr int kBlurSampleCount = (kBlurRadius * 2 + 1) * (kBlurRadius * 2 + 1);

QRgb blurPixel(const QImage &image, int x, int y)
{
    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 0;

    for (int offsetY = -kBlurRadius; offsetY <= kBlurRadius; ++offsetY) {
        const int sampleY = std::clamp(y + offsetY, 0, image.height() - 1);
        for (int offsetX = -kBlurRadius; offsetX <= kBlurRadius; ++offsetX) {
            const int sampleX = std::clamp(x + offsetX, 0, image.width() - 1);
            const QRgb color = image.pixel(sampleX, sampleY);
            red += qRed(color);
            green += qGreen(color);
            blue += qBlue(color);
            alpha += qAlpha(color);
        }
    }

    return qRgba(red / kBlurSampleCount,
                 green / kBlurSampleCount,
                 blue / kBlurSampleCount,
                 alpha / kBlurSampleCount);
}

QImage applyEffect(const QImage &sourceImage, EffectType effect)
{
    const QImage source = sourceImage.convertToFormat(QImage::Format_ARGB32);
    QImage result(source.size(), QImage::Format_ARGB32);

    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const QRgb sourceColor = source.pixel(x, y);
            QRgb resultColor = sourceColor;

            switch (effect) {
            case EffectType::None:
                break;
            case EffectType::Grayscale: {
                const int gray = (30 * qRed(sourceColor) + 59 * qGreen(sourceColor)
                                  + 11 * qBlue(sourceColor)) / 100;
                resultColor = qRgba(gray, gray, gray, qAlpha(sourceColor));
                break;
            }
            case EffectType::Invert:
                resultColor = qRgba(255 - qRed(sourceColor),
                                    255 - qGreen(sourceColor),
                                    255 - qBlue(sourceColor),
                                    qAlpha(sourceColor));
                break;
            case EffectType::Blur:
                resultColor = blurPixel(source, x, y);
                break;
            }

            result.setPixel(x, y, resultColor);
        }
    }

    return result;
}

} // namespace

void QtImageEffectWorker::process(QImage sourceImage,
                                  EffectType effect,
                                  quint64 requestId)
{
    emit processingFinished(applyEffect(sourceImage, effect), requestId);
}

QtAsyncEffectProcessor::QtAsyncEffectProcessor()
{
    // EffectType is a project enum, so Qt must know its name to copy it into
    // the queued request. QImage is also registered explicitly for clarity.
    qRegisterMetaType<EffectType>("EffectType");
    qRegisterMetaType<QImage>("QImage");

    worker_ = new QtImageEffectWorker;
    worker_->moveToThread(&workerThread_);

    connect(&workerThread_, &QThread::finished,
            worker_, &QObject::deleteLater);
    connect(worker_, &QtImageEffectWorker::processingFinished,
            this,
            [this](const QImage &processedImage, quint64 requestId) {
                if (completionHandler_)
                    completionHandler_(processedImage, requestId);
            },
            Qt::QueuedConnection);

    workerThread_.start();
}

QtAsyncEffectProcessor::~QtAsyncEffectProcessor()
{
    // The worker never touches an MFC or Qt widget. Tell its event loop to
    // exit, then wait only during application shutdown.
    workerThread_.quit();
    workerThread_.wait();
}

void QtAsyncEffectProcessor::requestProcessing(const QImage &sourceImage,
                                                EffectType effect,
                                                quint64 requestId)
{
    QMetaObject::invokeMethod(worker_, "process", Qt::QueuedConnection,
                              Q_ARG(QImage, sourceImage),
                              Q_ARG(EffectType, effect),
                              Q_ARG(quint64, requestId));
}

void QtAsyncEffectProcessor::setCompletionHandler(CompletionHandler handler)
{
    completionHandler_ = std::move(handler);
}
