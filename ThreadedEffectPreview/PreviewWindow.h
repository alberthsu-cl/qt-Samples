#pragma once

#include "EffectType.h"

#include <QMainWindow>
#include <QImage>
#include <QThread>

class FrameProcessor;
class QComboBox;
class QLabel;
class QPushButton;
class QResizeEvent;

class PreviewWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit PreviewWindow(QWidget *parent = nullptr);
    ~PreviewWindow() override;

signals:
    // Connected to FrameProcessor after it moves to workerThread_. Because the
    // objects have different thread affinity, Qt queues this call safely.
    void processRequested(int requestId, const QImage &image, EffectType effect);

private slots:
    void loadImage();
    void processCurrentImage();
    void showProcessedImage(int requestId,
                            const QImage &image,
                            const QString &workerThreadId);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    static QImage createDemoImage();
    void updatePreviews();
    void updateSourcePreview();
    void updateResultPreview();

    // UI-thread-only widgets and data.
    QLabel *sourceLabel_;
    QLabel *resultLabel_;
    QLabel *statusLabel_;
    QComboBox *effectCombo_;
    QPushButton *processButton_;
    QImage sourceImage_;
    QImage resultImage_;
    int nextRequestId_ = 1;
    bool processing_ = false;

    // The worker is created on the UI thread, then moved to workerThread_.
    QThread workerThread_;
    FrameProcessor *processor_;
};
