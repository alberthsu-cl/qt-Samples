#include "QtPropertiesPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QWidget *createSliderEditor(QSlider *slider, QSpinBox *spinBox, QWidget *parent)
{
    auto *editor = new QWidget(parent);
    auto *layout = new QHBoxLayout(editor);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(slider, 1);
    layout->addWidget(spinBox);
    return editor;
}

} // namespace

QtPropertiesPanel::QtPropertiesPanel(QWidget *parent)
    : QWidget(parent)
    , assetNameLabel_(new QLabel(this))
    , assetKindLabel_(new QLabel(this))
    , opacitySlider_(new QSlider(Qt::Horizontal, this))
    , opacitySpinBox_(new QSpinBox(this))
    , scaleSlider_(new QSlider(Qt::Horizontal, this))
    , scaleSpinBox_(new QSpinBox(this))
    , positionComboBox_(new QComboBox(this))
{
    setStyleSheet(QStringLiteral(
        "QtPropertiesPanel { background: #23252b; color: #e6e8ed; }"
        "QLabel { color: #e6e8ed; }"
        "QSpinBox, QComboBox { background: #31353e; color: #e6e8ed; "
        "border: 1px solid #4a4f5a; padding: 4px; }"));

    opacitySlider_->setRange(0, 100);
    opacitySpinBox_->setRange(0, 100);
    opacitySpinBox_->setSuffix(QStringLiteral(" %"));

    scaleSlider_->setRange(25, 200);
    scaleSpinBox_->setRange(25, 200);
    scaleSpinBox_->setSuffix(QStringLiteral(" %"));

    positionComboBox_->addItem(QStringLiteral("Center"),
                                static_cast<int>(ClipPosition::Center));
    positionComboBox_->addItem(QStringLiteral("Top Left"),
                                static_cast<int>(ClipPosition::TopLeft));
    positionComboBox_->addItem(QStringLiteral("Top Right"),
                                static_cast<int>(ClipPosition::TopRight));
    positionComboBox_->addItem(QStringLiteral("Bottom Left"),
                                static_cast<int>(ClipPosition::BottomLeft));
    positionComboBox_->addItem(QStringLiteral("Bottom Right"),
                                static_cast<int>(ClipPosition::BottomRight));

    auto *formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->addRow(QStringLiteral("Asset"), assetNameLabel_);
    formLayout->addRow(QStringLiteral("Type"), assetKindLabel_);
    formLayout->addRow(QStringLiteral("Opacity"),
                       createSliderEditor(opacitySlider_, opacitySpinBox_, this));
    formLayout->addRow(QStringLiteral("Scale"),
                       createSliderEditor(scaleSlider_, scaleSpinBox_, this));
    formLayout->addRow(QStringLiteral("Position"), positionComboBox_);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->addWidget(new QLabel(QStringLiteral("Clip Properties"), this));
    layout->addLayout(formLayout);
    layout->addStretch();

    // Each slider feeds its spin box. The spin box is the one place that
    // emits the semantic edit, which keeps a slider drag readable.
    connect(opacitySlider_, &QSlider::valueChanged,
            opacitySpinBox_, &QSpinBox::setValue);
    connect(opacitySpinBox_, qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                opacitySlider_->setValue(value);
                emitCurrentSettings();
            });
    connect(scaleSlider_, &QSlider::valueChanged,
            scaleSpinBox_, &QSpinBox::setValue);
    connect(scaleSpinBox_, qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                scaleSlider_->setValue(value);
                emitCurrentSettings();
            });
    connect(positionComboBox_, qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { emitCurrentSettings(); });
}

void QtPropertiesPanel::setSelectedAsset(const wchar_t *name, const wchar_t *kind,
                                         const ClipSettings &settings)
{
    assetNameLabel_->setText(QString::fromWCharArray(name));
    assetKindLabel_->setText(QString::fromWCharArray(kind));

    // MFC is applying the stored model state. Suppress each widget's signal so
    // selection changes never look like user edits returning to MFC.
    const QSignalBlocker blockOpacitySlider(opacitySlider_);
    const QSignalBlocker blockOpacitySpinBox(opacitySpinBox_);
    const QSignalBlocker blockScaleSlider(scaleSlider_);
    const QSignalBlocker blockScaleSpinBox(scaleSpinBox_);
    const QSignalBlocker blockPosition(positionComboBox_);
    opacitySlider_->setValue(settings.opacityPercent);
    opacitySpinBox_->setValue(settings.opacityPercent);
    scaleSlider_->setValue(settings.scalePercent);
    scaleSpinBox_->setValue(settings.scalePercent);
    positionComboBox_->setCurrentIndex(positionComboBox_->findData(
        static_cast<int>(settings.position)));
}

void QtPropertiesPanel::emitCurrentSettings()
{
    emit clipSettingsEdited(opacitySpinBox_->value(),
                            scaleSpinBox_->value(),
                            positionComboBox_->currentData().toInt());
}
