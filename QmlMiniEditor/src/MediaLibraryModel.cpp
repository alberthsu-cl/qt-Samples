#include "MediaLibraryModel.h"

MediaLibraryModel::MediaLibraryModel(QObject *parent)
    : QAbstractListModel(parent)
    , assets_({
          {QStringLiteral("Mountainbike.mp4"), QStringLiteral("VIDEO"), QStringLiteral("00:10"), QColor("#b97c2b")},
          {QStringLiteral("Forest.jpg"), QStringLiteral("IMAGE"), QStringLiteral("STILL"), QColor("#2d7a49")},
          {QStringLiteral("Mahoroba.mp3"), QStringLiteral("AUDIO"), QStringLiteral("02:19"), QColor("#2e7cb8")},
          {QStringLiteral("Sport.jpg"), QStringLiteral("IMAGE"), QStringLiteral("STILL"), QColor("#8a6a54")}
      })
{
}

int MediaLibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : assets_.size();
}

QVariant MediaLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= assets_.size())
        return {};

    const MediaAsset &asset = assets_.at(index.row());
    switch (role) {
    case TitleRole: return asset.title;
    case KindRole: return asset.kind;
    case DurationRole: return asset.duration;
    case AccentColorRole: return asset.accentColor;
    default: return {};
    }
}

QHash<int, QByteArray> MediaLibraryModel::roleNames() const
{
    return {
        {TitleRole, "title"},
        {KindRole, "kind"},
        {DurationRole, "duration"},
        {AccentColorRole, "accentColor"}
    };
}

QString MediaLibraryModel::titleAt(int row) const
{
    return row >= 0 && row < assets_.size() ? assets_.at(row).title : QString{};
}
