#pragma once

#include <QAbstractListModel>

// A Qt adapter over the framework-neutral DemoProject data. It deliberately
// has no MFC window knowledge and can later be shared by other Qt views.
class MediaAssetModel final : public QAbstractListModel
{
public:
    enum Role {
        AssetIndexRole = Qt::UserRole + 1,
        AssetKindRole,
        AssetDurationRole,
        ThumbnailColorRole
    };

    explicit MediaAssetModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
};
