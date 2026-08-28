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
#include <QPen>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStackedLayout>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kDragPixmapWidth = 84;

enum class MediaSourceFilter {
    AllItems,
    RealItems,
    FakeItems
};

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
        painter->fillRect(thumbnailRect,
                          index.data(MediaAssetModel::ThumbnailColorRole).value<QColor>());

        const QString kind = index.data(MediaAssetModel::AssetKindRole).toString();
        const QRect badgeRect(thumbnailRect.left() + 5, thumbnailRect.top() + 5, 48, 20);
        painter->fillRect(badgeRect, QColor(25, 27, 32));
        painter->setPen(QColor(235, 237, 242));
        painter->drawText(badgeRect, Qt::AlignCenter, kind);

        const bool isReal = index.data(MediaAssetModel::AssetIsRealRole).toBool();
        const QRect sourceBadgeRect(thumbnailRect.right() - 51, thumbnailRect.top() + 5,
                                    46, 20);
        painter->fillRect(sourceBadgeRect,
                          isReal ? QColor(39, 115, 75) : QColor(110, 77, 48));
        painter->setPen(QColor(235, 237, 242));
        painter->drawText(sourceBadgeRect, Qt::AlignCenter,
                          isReal ? QStringLiteral("Real")
                                 : QStringLiteral("Fake"));

        painter->setPen(QColor(235, 237, 242));
        const QRect nameRect(itemRect.left() + 5, itemRect.top() + 74,
                             itemRect.width() - 10, 22);
        painter->drawText(nameRect, Qt::AlignCenter | Qt::TextSingleLine,
                          index.data(Qt::DisplayRole).toString());

        painter->setPen(QColor(166, 171, 183));
        const QRect durationRect(itemRect.left() + 5, itemRect.top() + 96,
                                 itemRect.width() - 10, 18);
        painter->drawText(durationRect, Qt::AlignCenter | Qt::TextSingleLine,
                          index.data(MediaAssetModel::AssetDurationRole).toString());
        painter->restore();
    }
};

} // namespace

QtMediaLibraryPanel::QtMediaLibraryPanel(const MediaLibrary &mediaLibrary, QWidget *parent)
    : QWidget(parent)
    , assetModel_(new MediaAssetModel(mediaLibrary, this))
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
