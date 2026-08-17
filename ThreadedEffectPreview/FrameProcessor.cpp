#include "FrameProcessor.h"

#include <QThread>

namespace {

QString currentThreadIdText()
{
    return QString::number(
        reinterpret_cast<quintptr>(QThread::currentThreadId()), 16);
}

bool interruptionRequested()
{
    return QThread::currentThread()->isInterruptionRequested();
}

QImage applyGrayscale(const QImage &source, bool &wasInterrupted)
{
    QImage result = source.copy();

    for (int y = 0; y < result.height(); ++y) {
        if (interruptionRequested()) {
            wasInterrupted = true;
            return {};
        }

        auto *pixels = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = pixels[x];
            const int gray = qGray(pixel);
            pixels[x] = qRgba(gray, gray, gray, qAlpha(pixel));
        }
    }

    return result;
}

QImage applyInvert(const QImage &source, bool &wasInterrupted)
{
    QImage result = source.copy();

    for (int y = 0; y < result.height(); ++y) {
        if (interruptionRequested()) {
            wasInterrupted = true;
            return {};
        }

        auto *pixels = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = pixels[x];
            pixels[x] = qRgba(255 - qRed(pixel),
                              255 - qGreen(pixel),
                              255 - qBlue(pixel),
                              qAlpha(pixel));
        }
    }

    return result;
}

QImage applyBoxBlur(const QImage &source, bool &wasInterrupted)
{
    QImage result(source.size(), QImage::Format_ARGB32);

    // Average the 3 x 3 neighbourhood around each pixel. This is intentionally
    // CPU work: the goal of this sample is safe Qt thread communication.
    for (int y = 0; y < source.height(); ++y) {
        if (interruptionRequested()) {
            wasInterrupted = true;
            return {};
        }

        auto *resultPixels = reinterpret_cast<QRgb *>(result.scanLine(y));

        for (int x = 0; x < source.width(); ++x) {
            int red = 0;
            int green = 0;
            int blue = 0;
            int alpha = 0;
            int sampleCount = 0;

            for (int offsetY = -1; offsetY <= 1; ++offsetY) {
                const int sampleY = qBound(0, y + offsetY, source.height() - 1);
                const auto *sourcePixels = reinterpret_cast<const QRgb *>(source.constScanLine(sampleY));

                for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                    const int sampleX = qBound(0, x + offsetX, source.width() - 1);
                    const QRgb pixel = sourcePixels[sampleX];
                    red += qRed(pixel);
                    green += qGreen(pixel);
                    blue += qBlue(pixel);
                    alpha += qAlpha(pixel);
                    ++sampleCount;
                }
            }

            resultPixels[x] = qRgba(red / sampleCount,
                                    green / sampleCount,
                                    blue / sampleCount,
                                    alpha / sampleCount);
        }
    }

    return result;
}

} // namespace

void FrameProcessor::processImage(int requestId, const QImage &image, EffectType effect)
{
    // The queued signal carries an implicit-sharing QImage. Make a detached,
    // ARGB32 image before changing pixels, so the UI's source preview remains
    // independent and untouched.
    const QImage source = image.convertToFormat(QImage::Format_ARGB32);

    bool wasInterrupted = false;
    QImage result;
    switch (effect) {
    case EffectType::Grayscale:
        result = applyGrayscale(source, wasInterrupted);
        break;
    case EffectType::Invert:
        result = applyInvert(source, wasInterrupted);
        break;
    case EffectType::Blur:
        result = applyBoxBlur(source, wasInterrupted);
        break;
    }

    // During shutdown, no result needs to return to the UI. Returning from this
    // slot lets QThread's event loop finish quickly after quit() was requested.
    if (wasInterrupted)
        return;

    emit processingFinished(requestId, result, currentThreadIdText());
}
