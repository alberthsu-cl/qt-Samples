#include "MediaAssetModel.h"

#include "MediaLibrary.h"

#include <QColor>
#include <QMimeData>

#include <filesystem>

namespace {
constexpr char kMediaAssetMimeType[] = "application/x-mini-editor-media-id";
}

MediaAssetModel::MediaAssetModel(const MediaLibrary &mediaLibrary, QObject *parent)
    : QAbstractListModel(parent)
    , mediaLibrary_(mediaLibrary)
{
}

void MediaAssetModel::refresh()
{
    beginResetModel();
    endResetModel();
}

int MediaAssetModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(mediaLibrary_.assets().size());
}

QVariant MediaAssetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(mediaLibrary_.assets().size())) {
        return {};
    }

    const LibraryMediaAsset &asset = mediaLibrary_.assets()[index.row()];
    switch (role) {
    case Qt::DisplayRole:
        return QString::fromStdWString(asset.displayName);
    case AssetIndexRole:
        return index.row();
    case AssetIdRole:
        return asset.id;
    case AssetKindRole:
        return asset.kind == MediaKind::Audio ? QStringLiteral("Audio")
            : asset.kind == MediaKind::Image ? QStringLiteral("Image")
                                             : QStringLiteral("Video");
    case AssetDurationRole:
        return asset.kind == MediaKind::Image ? QStringLiteral("Still")
            : QStringLiteral("%1:%2")
                  .arg(asset.timelineDurationFrames / 1800, 2, 10, QLatin1Char('0'))
                  .arg((asset.timelineDurationFrames / 30) % 60, 2, 10, QLatin1Char('0'));
    case ThumbnailColorRole:
        return QColor::fromRgb(asset.thumbnailColorRgb);
    case AssetIsRealRole: {
        std::error_code error;
        return !asset.filePath.empty()
            && std::filesystem::is_regular_file(asset.filePath, error);
    }
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
