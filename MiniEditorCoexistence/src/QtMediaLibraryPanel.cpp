#include "QtMediaLibraryPanel.h"

#include "MediaAssetModel.h"
#include "MediaLibrary.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QDrag>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QListView>
#include <QMimeData>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRadialGradient>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStackedLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace {

constexpr int kDragPixmapWidth = 84;

enum class MediaSourceFilter {
    AllItems,
    RealItems,
    FakeItems
};

void drawAudioArtwork(QPainter *painter, const QRect &rect,
                      const QString &durationText)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(rect, QColor(5, 10, 18));

    const QPointF center(rect.center().x(), rect.center().y() + 2);
    QRadialGradient glow(center, rect.height() * 0.58);
    glow.setColorAt(0.0, QColor(0, 195, 235, 75));
    glow.setColorAt(0.55, QColor(0, 90, 190, 35));
    glow.setColorAt(1.0, QColor(4, 9, 17, 0));
    painter->fillRect(rect, glow);

    constexpr int kWaveformSegments = 72;
    constexpr qreal kPi = 3.14159265358979323846;
    const qreal baseRadius = std::min(rect.width(), rect.height()) * 0.27;
    painter->setPen(QPen(QColor(0, 204, 239, 210), 1.5,
                         Qt::SolidLine, Qt::RoundCap));
    for (int index = 0; index < kWaveformSegments; ++index) {
        const qreal angle = 2.0 * kPi * index / kWaveformSegments;
        const qreal wave = 2.0
            + 3.5 * (0.5 + 0.5 * std::sin(index * 1.71))
            + 1.8 * (0.5 + 0.5 * std::sin(index * 0.43));
        const QPointF direction(std::cos(angle), std::sin(angle));
        const QPointF inner = center + direction * (baseRadius - wave * 0.35);
        const QPointF outer = center + direction * (baseRadius + wave);
        painter->drawLine(inner, outer);
    }

    painter->setPen(QPen(QColor(0, 220, 245, 225), 1.4));
    painter->drawEllipse(center, baseRadius - 1.5, baseRadius - 1.5);

    // Draw the note as geometry so its appearance does not depend on the
    // user's installed fonts.
    painter->setPen(QPen(QColor(0, 220, 245), 2.4,
                         Qt::SolidLine, Qt::RoundCap));
    painter->setBrush(QColor(0, 220, 245));
    const QPointF noteHead(center.x() - 3.0, center.y() + 6.0);
    painter->drawEllipse(noteHead, 3.5, 2.7);
    painter->drawLine(QPointF(noteHead.x() + 3.0, noteHead.y()),
                      QPointF(noteHead.x() + 3.0, noteHead.y() - 12.0));
    QPainterPath flag;
    flag.moveTo(noteHead.x(), noteHead.y() - 11.0);
    flag.cubicTo(noteHead.x() + 9.0, noteHead.y() - 12.0,
                 noteHead.x() + 9.0, noteHead.y() - 5.0,
                 noteHead.x() + 3.0, noteHead.y() - 4.0);
    painter->drawPath(flag);

    const QRect durationRect(rect.left() + 4, rect.top() + 3, 38, 17);
    painter->fillRect(durationRect, QColor(4, 10, 19, 220));
    painter->setPen(QColor(220, 232, 244));
    QFont durationFont = painter->font();
    durationFont.setPointSizeF(std::max(7.0, durationFont.pointSizeF() - 1.0));
    painter->setFont(durationFont);
    painter->drawText(durationRect, Qt::AlignCenter, durationText);
    painter->restore();
}

class MediaSourceFilterModel final : public QSortFilterProxyModel
{
public:
    explicit MediaSourceFilterModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
    }

    void setMediaSourceFilter(MediaSourceFilter filter)
    {
        if (filter_ == filter)
            return;

        filter_ = filter;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override
    {
        if (filter_ == MediaSourceFilter::AllItems)
            return true;

        const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0,
                                                              sourceParent);
        const bool isReal = sourceIndex.data(MediaAssetModel::AssetIsRealRole)
                                .toBool();
        return filter_ == MediaSourceFilter::RealItems ? isReal : !isReal;
    }

private:
    // Imported project media is the useful starting point. Sample/fake media
    // remains available through the filter when it is deliberately needed.
    MediaSourceFilter filter_ = MediaSourceFilter::RealItems;
};

class MediaAssetListView final : public QListView
{
public:
    using QListView::QListView;

protected:
    void startDrag(Qt::DropActions supportedActions) override
    {
        const QModelIndex index = currentIndex();
        if (!index.isValid())
            return;

        QMimeData *mimeData = model()->mimeData(QModelIndexList{ index });
        if (mimeData == nullptr)
            return;

        QDrag drag(this);
        drag.setMimeData(mimeData);

        // Put the pointer on the thumbnail's left edge. The same pointer X is
        // used as the new timeline clip's start, so the drag image and the
        // insertion preview communicate one consistent placement rule.
        const QRect itemRect = visualRect(index);
        const QPixmap dragPixmap = viewport()->grab(itemRect).scaledToWidth(
            kDragPixmapWidth, Qt::SmoothTransformation);
        drag.setPixmap(dragPixmap);
        drag.setHotSpot(QPoint(0, dragPixmap.height() / 2));
        drag.exec(supportedActions, Qt::CopyAction);
    }
};

class MediaAssetDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(140, 122);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        const QRect itemRect = option.rect.adjusted(4, 4, -4, -4);
        const bool selected = option.state.testFlag(QStyle::State_Selected);

        // Asset cards need their own surface. Using the same color as the
        // surrounding dark workspace made unselected media look like loose
        // text floating on the window rather than selectable library items.
        const QColor cardBackground = selected
            ? QColor(33, 96, 167) : QColor(45, 48, 56);
        const QColor cardBorder = selected
            ? QColor(83, 166, 255) : QColor(57, 61, 71);
        painter->fillRect(itemRect, cardBackground);
        painter->setPen(QPen(cardBorder, 2));
        painter->drawRect(itemRect.adjusted(1, 1, -1, -1));

        const QRect thumbnailRect(itemRect.left() + 5, itemRect.top() + 5,
                                  itemRect.width() - 10, 65);
        const QString kind = index.data(MediaAssetModel::AssetKindRole).toString();
        const QString duration =
            index.data(MediaAssetModel::AssetDurationRole).toString();
        const bool isAudio = kind == QStringLiteral("Audio");
        const QImage thumbnail = index.data(MediaAssetModel::ThumbnailImageRole).value<QImage>();
        if (isAudio) {
            drawAudioArtwork(painter, thumbnailRect, duration);
        } else if (thumbnail.isNull()) {
            painter->fillRect(thumbnailRect,
                              index.data(MediaAssetModel::ThumbnailColorRole).value<QColor>());
        } else {
            QSize paintedSize = thumbnail.size();
            paintedSize.scale(thumbnailRect.size(), Qt::KeepAspectRatio);
            QRect paintedRect(QPoint(), paintedSize);
            paintedRect.moveCenter(thumbnailRect.center());
            painter->fillRect(thumbnailRect, QColor(18, 20, 24));
            painter->drawImage(paintedRect, thumbnail);
        }

        if (!isAudio) {
            const QRect badgeRect(thumbnailRect.left() + 5,
                                  thumbnailRect.top() + 5, 48, 20);
            painter->fillRect(badgeRect, QColor(25, 27, 32));
            painter->setPen(QColor(235, 237, 242));
            painter->drawText(badgeRect, Qt::AlignCenter, kind);
        }

        const bool isReal = index.data(MediaAssetModel::AssetIsRealRole).toBool();
        // A real thumbnail identifies its source visually. Only generated
        // sample assets need an explicit Fake badge to avoid misleading users.
        if (!isReal) {
            const QRect sourceBadgeRect(thumbnailRect.right() - 51, thumbnailRect.top() + 5,
                                        46, 20);
            painter->fillRect(sourceBadgeRect, QColor(110, 77, 48));
            painter->setPen(QColor(235, 237, 242));
            painter->drawText(sourceBadgeRect, Qt::AlignCenter, QStringLiteral("Fake"));
        }

        painter->setPen(QColor(235, 237, 242));
        const QRect nameRect(itemRect.left() + 5, itemRect.top() + 74,
                             itemRect.width() - 10, 22);
        painter->drawText(nameRect, Qt::AlignCenter | Qt::TextSingleLine,
                          index.data(Qt::DisplayRole).toString());

        painter->setPen(QColor(166, 171, 183));
        const QRect durationRect(itemRect.left() + 5, itemRect.top() + 96,
                                 itemRect.width() - 10, 18);
        if (!isAudio) {
            painter->drawText(durationRect, Qt::AlignCenter | Qt::TextSingleLine,
                              duration);
        }
        painter->restore();
    }
};

} // namespace

QtMediaLibraryPanel::QtMediaLibraryPanel(const MediaLibrary &mediaLibrary, QWidget *parent,
                                         QtThumbnailCache *thumbnailCache)
    : QWidget(parent)
    , assetModel_(new MediaAssetModel(mediaLibrary, thumbnailCache, this))
    , assetFilterModel_(new MediaSourceFilterModel(this))
    , assetView_(new MediaAssetListView(this))
    , emptyImportButton_(new QToolButton(this))
    , assetContentLayout_(new QStackedLayout)
{
    setStyleSheet(QStringLiteral(
        "QtMediaLibraryPanel { background: #272a31; }"
        "QComboBox { background: #31353e; color: #e6e8ed; "
        "border: 1px solid #4a4f5a; padding: 5px; }"
        "QListView { background: #202228; border: none; }"));

    auto *sourceFilterSelector = new QComboBox(this);
    sourceFilterSelector->setObjectName(QStringLiteral("mediaSourceFilterComboBox"));
    sourceFilterSelector->addItem(QStringLiteral("All"),
                                  static_cast<int>(MediaSourceFilter::AllItems));
    sourceFilterSelector->addItem(QStringLiteral("Real"),
                                  static_cast<int>(MediaSourceFilter::RealItems));
    sourceFilterSelector->addItem(QStringLiteral("Fake"),
                                  static_cast<int>(MediaSourceFilter::FakeItems));
    sourceFilterSelector->setCurrentIndex(1);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(sourceFilterSelector, 1);

    auto *importButton = new QPushButton(QStringLiteral("Import"), this);
    auto *removeButton = new QPushButton(QStringLiteral("Remove"), this);
    importButton->setObjectName(QStringLiteral("importButton"));
    removeButton->setObjectName(QStringLiteral("removeButton"));
    headerLayout->addWidget(importButton);
    headerLayout->addWidget(removeButton);

    assetFilterModel_->setSourceModel(assetModel_);
    assetView_->setModel(assetFilterModel_);
    assetView_->setObjectName(QStringLiteral("assetView"));
    assetView_->setItemDelegate(new MediaAssetDelegate(assetView_));
    assetView_->setViewMode(QListView::IconMode);
    assetView_->setResizeMode(QListView::Adjust);
    assetView_->setUniformItemSizes(true);
    assetView_->setSpacing(5);
    assetView_->setSelectionMode(QAbstractItemView::SingleSelection);
    assetView_->setDragEnabled(true);
    assetView_->setDragDropMode(QAbstractItemView::DragOnly);

    // Keep the empty library quiet and direct: the centered folder icon is
    // the only action. There is no explanatory link or drag/drop affordance.
    emptyImportButton_->setObjectName(QStringLiteral("emptyImportButton"));
    emptyImportButton_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    emptyImportButton_->setIconSize(QSize(48, 48));
    emptyImportButton_->setFixedSize(96, 96);
    emptyImportButton_->setToolTip(QStringLiteral("Import media"));
    emptyImportButton_->setStyleSheet(QStringLiteral(
        "QToolButton { background: #30343d; border: 1px solid #525865; }"
        "QToolButton:hover { background: #3c5572; border-color: #69adf5; }"));

    assetContentLayout_->setContentsMargins(0, 0, 0, 0);
    assetContentLayout_->addWidget(assetView_);
    assetContentLayout_->addWidget(emptyImportButton_);
    assetContentLayout_->setAlignment(emptyImportButton_, Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    layout->addLayout(headerLayout);
    layout->addLayout(assetContentLayout_, 1);

    connect(assetView_->selectionModel(), &QItemSelectionModel::currentChanged,
            this,
            [this](const QModelIndex &current, const QModelIndex &) {
                if (current.isValid())
                    emit assetSelected(current.data(MediaAssetModel::AssetIndexRole).toInt());
            });
    connect(importButton, &QPushButton::clicked, this, &QtMediaLibraryPanel::importRequested);
    connect(emptyImportButton_, &QToolButton::clicked,
            this, &QtMediaLibraryPanel::importRequested);
    connect(sourceFilterSelector, &QComboBox::currentIndexChanged, this,
            [this, sourceFilterSelector](int index) {
                const auto filter = static_cast<MediaSourceFilter>(
                    sourceFilterSelector->itemData(index).toInt());
                clearSelection();
                static_cast<MediaSourceFilterModel *>(assetFilterModel_)
                    ->setMediaSourceFilter(filter);
                updateEmptyState();
            });
    connect(removeButton, &QPushButton::clicked, this, [this] {
        const QModelIndex current = assetView_->currentIndex();
        if (!current.isValid())
            return;
        emit removeRequested(current.data(MediaAssetModel::AssetIndexRole).toInt(),
                             current.data(MediaAssetModel::AssetIdRole).toInt());
    });

    connect(assetFilterModel_, &QAbstractItemModel::modelReset,
            this, &QtMediaLibraryPanel::updateEmptyState);
    connect(assetFilterModel_, &QAbstractItemModel::rowsInserted,
            this, [this] { updateEmptyState(); });
    connect(assetFilterModel_, &QAbstractItemModel::rowsRemoved,
            this, [this] { updateEmptyState(); });
    updateEmptyState();
}

void QtMediaLibraryPanel::refreshAssets()
{
    assetModel_->refresh();
    updateEmptyState();
}

void QtMediaLibraryPanel::clearSelection()
{
    const QSignalBlocker signalBlocker(assetView_->selectionModel());
    assetView_->clearSelection();
    assetView_->setCurrentIndex({});
}

void QtMediaLibraryPanel::setSelectedAssetIndex(int assetIndex)
{
    const QModelIndex index = assetFilterModel_->mapFromSource(
        assetModel_->index(assetIndex, 0));
    if (!index.isValid())
        return;

    // MFC is synchronizing the selection. Do not turn it into a second user
    // selection notification back across the coexistence boundary.
    const QSignalBlocker signalBlocker(assetView_->selectionModel());
    assetView_->setCurrentIndex(index);
    assetView_->selectionModel()->select(index,
                                         QItemSelectionModel::ClearAndSelect
                                             | QItemSelectionModel::Rows);
}

void QtMediaLibraryPanel::updateEmptyState()
{
    assetContentLayout_->setCurrentWidget(assetFilterModel_->rowCount() == 0
        ? static_cast<QWidget *>(emptyImportButton_)
        : static_cast<QWidget *>(assetView_));
}
