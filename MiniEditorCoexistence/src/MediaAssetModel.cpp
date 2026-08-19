#include "MediaAssetModel.h"

#include "DemoProject.h"

#include <QColor>

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
