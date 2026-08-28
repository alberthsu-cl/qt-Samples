#pragma once

#include <QAbstractListModel>

class QMimeData;
class MediaLibrary;

// A Qt adapter over the framework-neutral DemoProject data. It deliberately
// has no MFC window knowledge and can later be shared by other Qt views.
class MediaAssetModel final : public QAbstractListModel
{
public:
    enum Role {
        AssetIndexRole = Qt::UserRole + 1,
        AssetIdRole,
        AssetKindRole,
        AssetDurationRole,
        ThumbnailColorRole,
        AssetIsRealRole
    };

    explicit MediaAssetModel(const MediaLibrary &mediaLibrary, QObject *parent = nullptr);

    void refresh();

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;

private:
    const MediaLibrary &mediaLibrary_;
};
