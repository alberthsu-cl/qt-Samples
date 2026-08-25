#include "MediaAssetModel.h"

#include "DemoProject.h"

#include <QColor>
#include <QMimeData>

namespace {
constexpr char kMediaAssetMimeType[] = "application/x-mini-editor-media-id";
}

MediaAssetModel::MediaAssetModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MediaAssetModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(demoAssets().size());
}

QVariant MediaAssetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(demoAssets().size())) {
        return {};
    }

    const auto &asset = demoAssets()[index.row()];
    switch (role) {
    case Qt::DisplayRole:
        return QString::fromWCharArray(asset.name);
    case AssetIndexRole:
        return index.row();
    case AssetIdRole:
        return asset.id;
    case AssetKindRole:
        return QString::fromWCharArray(asset.kind);
    case AssetDurationRole:
        return QString::fromWCharArray(asset.duration);
    case ThumbnailColorRole:
        return QColor(GetRValue(asset.thumbnailColor),
                      GetGValue(asset.thumbnailColor),
                      GetBValue(asset.thumbnailColor));
    default:
        return {};
    }
}

Qt::ItemFlags MediaAssetModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);
    if (index.isValid())
        defaultFlags |= Qt::ItemIsDragEnabled;
    return defaultFlags;
}

QStringList MediaAssetModel::mimeTypes() const
{
    return { QString::fromLatin1(kMediaAssetMimeType) };
}

QMimeData *MediaAssetModel::mimeData(const QModelIndexList &indexes) const
{
    if (indexes.empty() || !indexes.front().isValid())
        return nullptr;

    auto *mimeData = new QMimeData;
    mimeData->setData(QString::fromLatin1(kMediaAssetMimeType),
                      QByteArray::number(indexes.front().data(AssetIdRole).toInt()));
    return mimeData;
}
