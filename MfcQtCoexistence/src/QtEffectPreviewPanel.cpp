#include "QtEffectPreviewPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

QtEffectPreviewPanel::QtEffectPreviewPanel(QWidget *parent)
    : QWidget(parent)
    , imageLabel_(new QLabel(this))
    , controlsArea_(new QWidget(this))
    , toggleButton_(new QToolButton(this))
{
    setStyleSheet(QStringLiteral("QtEffectPreviewPanel { background: #141414; }"));

    imageLabel_->setAlignment(Qt::AlignCenter);
    // The scaled pixmap must not influence the label's preferred size.
    // Otherwise, a resize can feed back into another layout and repaint pass.
    imageLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel_->setStyleSheet(QStringLiteral(
        "QLabel { background: #141414; color: #d0d0d0; border: 1px solid #383838; }"));
    imageLabel_->setText(QStringLiteral("No image loaded."));

    // Keep the controls visually distinct from the image canvas. This makes
    // the embedded Qt child read as a small preview panel, not a second frame.
    controlsArea_->setStyleSheet(QStringLiteral(
        "QWidget { background: #242424; border-top: 1px solid #454545; }"));
    controlsArea_->setFixedHeight(36);

    toggleButton_->setCheckable(true);
    toggleButton_->setChecked(true);
    toggleButton_->setFixedSize(42, 32);
    connect(toggleButton_, &QToolButton::toggled, this,
            [this](bool isChecked) {
                showingProcessedImage_ = isChecked;
                updateToggleAppearance();
                updateDisplayedImage();
                emit displayModeChanged(showingProcessedImage_);
            });

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);
    mainLayout->addWidget(imageLabel_, 1);

    auto *controlsLayout = new QHBoxLayout(controlsArea_);
    controlsLayout->setContentsMargins(8, 2, 8, 2);
    controlsLayout->addStretch();
    controlsLayout->addWidget(toggleButton_);
    mainLayout->addWidget(controlsArea_);

    updateToggleAppearance();
}

void QtEffectPreviewPanel::setImages(const QImage &originalImage,
                                     const QImage &processedImage)
{
    originalImage_ = originalImage;
    processedImage_ = processedImage;
    updateDisplayedImage();
}

void QtEffectPreviewPanel::setShowingProcessedImage(bool showingProcessedImage)
{
    showingProcessedImage_ = showingProcessedImage;

    // The MFC frame is synchronizing the view. Block toggled() so this change
    // does not look like a second, user-initiated state change.
    const QSignalBlocker signalBlocker(toggleButton_);
    toggleButton_->setChecked(showingProcessedImage_);
    updateToggleAppearance();
    updateDisplayedImage();
}

void QtEffectPreviewPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateDisplayedImage();
}

void QtEffectPreviewPanel::updateDisplayedImage()
{
    const QImage &image = showingProcessedImage_ ? processedImage_ : originalImage_;
    if (image.isNull()) {
        imageLabel_->setPixmap(QPixmap());
        imageLabel_->setText(QStringLiteral("No image loaded."));
        return;
    }

    imageLabel_->setText({});
    imageLabel_->setPixmap(QPixmap::fromImage(image).scaled(
        imageLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void QtEffectPreviewPanel::updateToggleAppearance()
{
    const bool isChecked = showingProcessedImage_;
    toggleButton_->setIcon(createToggleIcon(isChecked));
    toggleButton_->setToolTip(isChecked
                                  ? QStringLiteral("Applied effect is displayed")
                                  : QStringLiteral("Original image is displayed"));
}

QIcon QtEffectPreviewPanel::createToggleIcon(bool isChecked)
{
    QPixmap iconPixmap(24, 24);
    iconPixmap.fill(Qt::transparent);

    QPainter painter(&iconPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(70, 70, 70), 2));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(QRect(3, 3, 18, 18), 2, 2);

    if (isChecked) {
        painter.setPen(QPen(QColor(30, 150, 65), 3,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPoint(6, 12), QPoint(10, 16));
        painter.drawLine(QPoint(10, 16), QPoint(18, 7));
    }

    return QIcon(iconPixmap);
}
