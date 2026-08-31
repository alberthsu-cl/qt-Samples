#include "QtFrameEffectProcessor.h"

#include <QThread>

#include <algorithm>

namespace {

bool interruptionRequested()
{
    QThread *thread = QThread::currentThread();
    return thread != nullptr && thread->isInterruptionRequested();
}

int blendChannel(int original, int processed, int intensityPercent)
{
    return original + (processed - original) * intensityPercent / 100;
}

QRgb blendPixel(QRgb original, int red, int green, int blue, int intensityPercent)
{
    return qRgba(blendChannel(qRed(original), red, intensityPercent),
                 blendChannel(qGreen(original), green, intensityPercent),
                 blendChannel(qBlue(original), blue, intensityPercent),
                 qAlpha(original));
}

QImage applyGrayscale(const QImage &source, int intensityPercent, bool &wasInterrupted)
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
            pixels[x] = blendPixel(pixel, gray, gray, gray, intensityPercent);
        }
    }
    return result;
}

QImage applyInvert(const QImage &source, int intensityPercent, bool &wasInterrupted)
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
            pixels[x] = blendPixel(pixel, 255 - qRed(pixel), 255 - qGreen(pixel),
                                   255 - qBlue(pixel), intensityPercent);
        }
    }
    return result;
}

QImage applyBoxBlur(const QImage &source, int intensityPercent, bool &wasInterrupted)
{
    QImage result(source.size(), QImage::Format_ARGB32);

    // Average the 3 x 3 neighbourhood around each pixel, exactly as the
    // ThreadedEffectPreview sample does. It is intentionally plain CPU work:
    // the lesson being carried over is the thread boundary, not the kernel.
    for (int y = 0; y < source.height(); ++y) {
        if (interruptionRequested()) {
            wasInterrupted = true;
            return {};
        }

        auto *resultPixels = reinterpret_cast<QRgb *>(result.scanLine(y));
        const auto *originalPixels =
            reinterpret_cast<const QRgb *>(source.constScanLine(y));

        for (int x = 0; x < source.width(); ++x) {
            int red = 0;
            int green = 0;
            int blue = 0;
            int sampleCount = 0;

            for (int offsetY = -1; offsetY <= 1; ++offsetY) {
                const int sampleY = std::clamp(y + offsetY, 0, source.height() - 1);
                const auto *sourcePixels =
                    reinterpret_cast<const QRgb *>(source.constScanLine(sampleY));

                for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                    const int sampleX = std::clamp(x + offsetX, 0, source.width() - 1);
                    const QRgb pixel = sourcePixels[sampleX];
                    red += qRed(pixel);
                    green += qGreen(pixel);
                    blue += qBlue(pixel);
                    ++sampleCount;
                }
            }

            resultPixels[x] = blendPixel(originalPixels[x], red / sampleCount,
                                         green / sampleCount, blue / sampleCount,
                                         intensityPercent);
        }
    }
    return result;
}

} // namespace

QImage QtFrameEffectProcessor::applyEffect(const QImage &frame, ClipEffectKind effect,
                                           int intensityPercent, bool *wasInterrupted)
{
    bool interrupted = false;
    const int intensity = std::clamp(intensityPercent,
                                     kMinimumEffectIntensityPercent,
                                     kMaximumEffectIntensityPercent);
    if (effect == ClipEffectKind::None || intensity == 0 || frame.isNull()) {
        if (wasInterrupted != nullptr)
            *wasInterrupted = false;
        return frame;
    }

    // The queued signal carries an implicitly shared QImage. Detaching to
    // ARGB32 first keeps the preview's own copy independent of this worker.
    const QImage source = frame.convertToFormat(QImage::Format_ARGB32);

    QImage result;
    switch (effect) {
    case ClipEffectKind::Grayscale:
        result = applyGrayscale(source, intensity, interrupted);
        break;
    case ClipEffectKind::Invert:
        result = applyInvert(source, intensity, interrupted);
        break;
    case ClipEffectKind::Blur:
        result = applyBoxBlur(source, intensity, interrupted);
        break;
    case ClipEffectKind::None:
        break;
    }

    if (wasInterrupted != nullptr)
        *wasInterrupted = interrupted;
    return interrupted ? QImage() : result;
}

void QtFrameEffectProcessor::processFrame(int requestId, const QImage &frame,
                                          ClipEffectKind effect, int intensityPercent)
{
    bool wasInterrupted = false;
    const QImage result = applyEffect(frame, effect, intensityPercent, &wasInterrupted);

    // During shutdown no result needs to return to the UI. Returning lets the
    // worker thread's event loop finish quickly after quit() was requested.
    if (wasInterrupted)
        return;

    emit frameProcessed(requestId, result);
}
