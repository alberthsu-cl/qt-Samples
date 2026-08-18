#pragma once

#include <QImage>
#include <QWidget>

class QLabel;
class QIcon;
class QResizeEvent;
class QToolButton;
class QWidget;

// The next Qt migration unit: a complete, self-contained preview rectangle.
// It owns Qt layout, painting through QLabel/QPixmap, and the comparison
// toggle. It does not know anything about MFC or CImage.
class QtEffectPreviewPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit QtEffectPreviewPanel(QWidget *parent = nullptr);

    void setImages(const QImage &originalImage, const QImage &processedImage);
    void setShowingProcessedImage(bool showingProcessedImage);

signals:
    // Emitted only for a user click. Programmatic synchronization from MFC
    // deliberately does not echo this signal back to the MFC frame.
    void displayModeChanged(bool showingProcessedImage);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateDisplayedImage();
    void updateToggleAppearance();
    static QIcon createToggleIcon(bool isChecked);

    QLabel *imageLabel_ = nullptr;
    QWidget *controlsArea_ = nullptr;
    QToolButton *toggleButton_ = nullptr;
    QImage originalImage_;
    QImage processedImage_;
    bool showingProcessedImage_ = true;
};
