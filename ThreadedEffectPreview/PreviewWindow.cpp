#include "PreviewWindow.h"

#include "FrameProcessor.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QString currentThreadIdText()
{
    return QString::number(
        reinterpret_cast<quintptr>(QThread::currentThreadId()), 16);
}

QLabel *createPreviewLabel(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumSize(360, 240);
    label->setStyleSheet("QLabel { background: #202020; color: #d0d0d0; border: 1px solid #505050; }");
    return label;
}

} // namespace

PreviewWindow::PreviewWindow(QWidget *parent)
    : QMainWindow(parent)
    , sourceLabel_(createPreviewLabel(this))
    , resultLabel_(createPreviewLabel(this))
    , statusLabel_(new QLabel(this))
    , effectCombo_(new QComboBox(this))
    , processButton_(new QPushButton("Process on worker thread", this))
    , sourceImage_(createDemoImage())
    , processor_(new FrameProcessor)
{
    setWindowTitle("Qt Threaded Effect Preview");
    resize(1000, 640);

    // The worker becomes owned by the worker thread for slot execution.
    processor_->moveToThread(&workerThread_);
    connect(this, &PreviewWindow::processRequested,
            processor_, &FrameProcessor::processImage);
    connect(processor_, &FrameProcessor::processingFinished,
            this, &PreviewWindow::showProcessedImage);
    connect(&workerThread_, &QThread::finished,
            processor_, &QObject::deleteLater);
    workerThread_.start();

    effectCombo_->addItem("Grayscale", QVariant::fromValue(EffectType::Grayscale));
    effectCombo_->addItem("Invert", QVariant::fromValue(EffectType::Invert));
    effectCombo_->addItem("3 x 3 blur", QVariant::fromValue(EffectType::Blur));

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    auto *previewLayout = new QHBoxLayout;
    auto *controlsLayout = new QHBoxLayout;
    auto *loadButton = new QPushButton("Load image...", this);

    previewLayout->addWidget(sourceLabel_);
    previewLayout->addWidget(resultLabel_);
    controlsLayout->addWidget(loadButton);
    controlsLayout->addWidget(new QLabel("CPU effect:", this));
    controlsLayout->addWidget(effectCombo_);
    controlsLayout->addWidget(processButton_);
    controlsLayout->addStretch();

    mainLayout->addLayout(previewLayout, 1);
    mainLayout->addWidget(statusLabel_);
    mainLayout->addLayout(controlsLayout);
    setCentralWidget(centralWidget);

    connect(loadButton, &QPushButton::clicked, this, &PreviewWindow::loadImage);
    connect(processButton_, &QPushButton::clicked,
            this, &PreviewWindow::processCurrentImage);

    statusLabel_->setText("UI thread: 0x" + currentThreadIdText()
                          + ". Choose an effect, then send an image to the worker.");
    updatePreviews();
}

PreviewWindow::~PreviewWindow()
{
    // Interruption is cooperative: FrameProcessor checks the flag once per
    // image row. quit() then stops the event loop after the active slot exits.
    workerThread_.requestInterruption();
    workerThread_.quit();
    workerThread_.wait();
}

void PreviewWindow::loadImage()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open image",
        {},
        "Images (*.png *.jpg *.jpeg *.bmp);;All files (*.*)");

    if (fileName.isEmpty())
        return;

    QImageReader reader(fileName);
    const QImage image = reader.read();
    if (image.isNull()) {
        statusLabel_->setText("Could not read: " + fileName);
        return;
    }

    sourceImage_ = image;
    resultImage_ = QImage();
    resultLabel_->setText("Process this image on the worker thread.");
    updatePreviews();
}

void PreviewWindow::processCurrentImage()
{
    if (processing_ || sourceImage_.isNull())
        return;

    processing_ = true;
    processButton_->setEnabled(false);

    const int requestId = nextRequestId_++;
    const auto effect = effectCombo_->currentData().value<EffectType>();

    // copy() gives the worker an independent snapshot. The UI can safely load
    // another image while the worker reads and transforms this one.
    emit processRequested(requestId, sourceImage_.copy(), effect);
    statusLabel_->setText("Request " + QString::number(requestId)
                          + " was queued from UI thread 0x" + currentThreadIdText());
}

void PreviewWindow::showProcessedImage(int requestId,
                                       const QImage &image,
                                       const QString &workerThreadId)
{
    // This slot runs back on the UI thread, so it is safe to update widgets.
    resultImage_ = image;
    processing_ = false;
    processButton_->setEnabled(true);
    updateResultPreview();

    statusLabel_->setText("Request " + QString::number(requestId)
                          + " finished on worker thread 0x" + workerThreadId
                          + "; UI updated on thread 0x" + currentThreadIdText());
}

void PreviewWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updatePreviews();
}

QImage PreviewWindow::createDemoImage()
{
    QImage image(960, 540, QImage::Format_ARGB32);
    QPainter painter(&image);

    QLinearGradient gradient(0, 0, image.width(), image.height());
    gradient.setColorAt(0.0, QColor("#245a9c"));
    gradient.setColorAt(0.5, QColor("#e5a53a"));
    gradient.setColorAt(1.0, QColor("#a43b6e"));
    painter.fillRect(image.rect(), gradient);

    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(28);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(image.rect(), Qt::AlignCenter,
                     "Source image\nprocessed on a worker thread");
    return image;
}

void PreviewWindow::updatePreviews()
{
    updateSourcePreview();
    updateResultPreview();
}

void PreviewWindow::updateSourcePreview()
{
    if (sourceImage_.isNull())
        return;

    sourceLabel_->setPixmap(QPixmap::fromImage(sourceImage_).scaled(
        sourceLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void PreviewWindow::updateResultPreview()
{
    if (resultImage_.isNull())
        return;

    resultLabel_->setPixmap(QPixmap::fromImage(resultImage_).scaled(
        resultLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
