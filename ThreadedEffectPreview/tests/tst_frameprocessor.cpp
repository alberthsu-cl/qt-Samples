#include "EffectType.h"
#include "FrameProcessor.h"

#include <QMetaObject>
#include <QSignalSpy>
#include <QThread>
#include <QtTest>

#include <optional>

class FrameProcessorTest final : public QObject
{
    Q_OBJECT

private:
    struct ProcessResult {
        int requestId = 0;
        QImage image;
        QString workerThreadId;
    };

    std::optional<ProcessResult> processOnWorkerThread(const QImage &source,
                                                        EffectType effect);

private slots:
    void grayscale_changesChannelsAndUsesWorkerThread();
    void invert_changesChannelsAndUsesWorkerThread();
    void blur_averagesTheNeighbourhood();
};

std::optional<FrameProcessorTest::ProcessResult> FrameProcessorTest::processOnWorkerThread(
    const QImage &source,
    EffectType effect)
{
    QThread workerThread;
    auto *processor = new FrameProcessor;
    QSignalSpy completionSpy(processor, &FrameProcessor::processingFinished);
    if (!completionSpy.isValid()) {
        delete processor;
        return std::nullopt;
    }

    processor->moveToThread(&workerThread);

    // This connection makes the worker object clean itself up when its thread
    // stops. The test never accesses processor after workerThread.wait().
    connect(&workerThread, &QThread::finished, processor, &QObject::deleteLater);

    workerThread.start();

    constexpr int requestId = 42;
    // QueuedConnection is the important part: this lambda runs on the event
    // loop owned by workerThread, not on the test's main thread.
    const bool wasQueued = QMetaObject::invokeMethod(
        processor,
        [processor, requestId, source, effect] {
            processor->processImage(requestId, source, effect);
        },
        Qt::QueuedConnection);
    if (!wasQueued) {
        workerThread.quit();
        workerThread.wait(3000);
        return std::nullopt;
    }

    // Do not use sleep(). QSignalSpy pumps the event loop until the signal is
    // received or a useful timeout expires.
    if (!completionSpy.wait(3000) || completionSpy.count() != 1) {
        workerThread.quit();
        workerThread.wait(3000);
        return std::nullopt;
    }

    const QList<QVariant> signalArguments = completionSpy.takeFirst();
    if (signalArguments.size() != 3 || signalArguments.at(0).toInt() != requestId) {
        workerThread.quit();
        workerThread.wait(3000);
        return std::nullopt;
    }

    ProcessResult result;
    result.requestId = requestId;
    result.image = qvariant_cast<QImage>(signalArguments.at(1));
    result.workerThreadId = signalArguments.at(2).toString();

    workerThread.quit();
    if (!workerThread.wait(3000))
        return std::nullopt;
    return result;
}

void FrameProcessorTest::grayscale_changesChannelsAndUsesWorkerThread()
{
    QImage source(1, 1, QImage::Format_ARGB32);
    source.setPixel(0, 0, qRgba(20, 100, 200, 77));

    const auto result = processOnWorkerThread(source, EffectType::Grayscale);
    QVERIFY2(result.has_value(), "The grayscale request did not finish cleanly.");
    const QRgb pixel = result->image.pixel(0, 0);

    const int expectedGray = qGray(source.pixel(0, 0));
    QCOMPARE(qRed(pixel), expectedGray);
    QCOMPARE(qGreen(pixel), expectedGray);
    QCOMPARE(qBlue(pixel), expectedGray);
    QCOMPARE(qAlpha(pixel), 77);
    QVERIFY(result->workerThreadId != QString::number(
        reinterpret_cast<quintptr>(QThread::currentThreadId()), 16));
}

void FrameProcessorTest::invert_changesChannelsAndUsesWorkerThread()
{
    QImage source(1, 1, QImage::Format_ARGB32);
    source.setPixel(0, 0, qRgba(10, 50, 200, 128));

    const auto result = processOnWorkerThread(source, EffectType::Invert);
    QVERIFY2(result.has_value(), "The invert request did not finish cleanly.");
    const QRgb pixel = result->image.pixel(0, 0);

    QCOMPARE(qRed(pixel), 245);
    QCOMPARE(qGreen(pixel), 205);
    QCOMPARE(qBlue(pixel), 55);
    QCOMPARE(qAlpha(pixel), 128);
    QVERIFY(!result->workerThreadId.isEmpty());
}

void FrameProcessorTest::blur_averagesTheNeighbourhood()
{
    QImage source(3, 3, QImage::Format_ARGB32);
    source.fill(qRgba(0, 0, 0, 255));
    source.setPixel(1, 1, qRgba(255, 0, 0, 255));

    const auto result = processOnWorkerThread(source, EffectType::Blur);
    QVERIFY2(result.has_value(), "The blur request did not finish cleanly.");
    const QRgb center = result->image.pixel(1, 1);

    // The center sees eight black pixels and one red pixel: 255 / 9 == 28.
    QCOMPARE(qRed(center), 28);
    QCOMPARE(qGreen(center), 0);
    QCOMPARE(qBlue(center), 0);
    QCOMPARE(qAlpha(center), 255);
}

QTEST_GUILESS_MAIN(FrameProcessorTest)

#include "tst_frameprocessor.moc"
