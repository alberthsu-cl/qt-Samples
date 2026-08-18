#include "QtAsyncEffectProcessor.h"

#include <QCoreApplication>
#include <QThread>
#include <QtTest>

namespace {

QImage makePixelImage(QRgb color)
{
    QImage image(1, 1, QImage::Format_ARGB32);
    image.setPixel(0, 0, color);
    return image;
}

} // namespace

class QtAsyncEffectProcessorTests final : public QObject
{
    Q_OBJECT

private slots:
    void grayscaleResultHasExpectedPixelsAndReturnsToUiThread();
    void staleResultIsIgnoredByTheUiRequestIdPolicy();
};

void QtAsyncEffectProcessorTests::grayscaleResultHasExpectedPixelsAndReturnsToUiThread()
{
    QtAsyncEffectProcessor processor;
    bool completed = false;
    QImage result;
    QThread *completionThread = nullptr;

    processor.setCompletionHandler(
        [&](QImage processedImage, quint64 requestId) {
            QCOMPARE(requestId, quint64{10});
            result = processedImage;
            completionThread = QThread::currentThread();
            completed = true;
        });

    processor.requestProcessing(makePixelImage(qRgba(200, 100, 50, 99)),
                                EffectType::Grayscale,
                                10);

    // QTRY_VERIFY runs the Qt test event loop while waiting. A blocking wait
    // here would prevent the queued completion from reaching this UI thread.
    QTRY_VERIFY_WITH_TIMEOUT(completed, 2000);

    QCOMPARE(completionThread, QCoreApplication::instance()->thread());
    const QRgb pixel = result.pixel(0, 0);
    QCOMPARE(qRed(pixel), 124);
    QCOMPARE(qGreen(pixel), 124);
    QCOMPARE(qBlue(pixel), 124);
    QCOMPARE(qAlpha(pixel), 99);
}

void QtAsyncEffectProcessorTests::staleResultIsIgnoredByTheUiRequestIdPolicy()
{
    QtAsyncEffectProcessor processor;
    constexpr quint64 currentRequestId = 2;
    int ignoredResultCount = 0;
    bool currentResultReceived = false;
    QImage currentResult;

    processor.setCompletionHandler(
        [&](QImage processedImage, quint64 requestId) {
            // This is the same guard used by MainFrame: worker work is not
            // cancelled, but an old completion cannot overwrite a new choice.
            if (requestId != currentRequestId) {
                ++ignoredResultCount;
                return;
            }

            currentResult = processedImage;
            currentResultReceived = true;
        });

    QImage blurInput(96, 96, QImage::Format_ARGB32);
    blurInput.fill(qRgb(30, 50, 70));
    processor.requestProcessing(blurInput, EffectType::Blur, 1);
    processor.requestProcessing(makePixelImage(qRgb(20, 40, 60)),
                                EffectType::Invert,
                                currentRequestId);

    QTRY_VERIFY_WITH_TIMEOUT(currentResultReceived, 3000);
    QCOMPARE(ignoredResultCount, 1);

    const QRgb pixel = currentResult.pixel(0, 0);
    QCOMPARE(qRed(pixel), 235);
    QCOMPARE(qGreen(pixel), 215);
    QCOMPARE(qBlue(pixel), 195);
}

QTEST_GUILESS_MAIN(QtAsyncEffectProcessorTests)

#include "QtAsyncEffectProcessorTests.moc"
