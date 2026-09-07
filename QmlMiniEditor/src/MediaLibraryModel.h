#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QVector>
#include <QtQml/qqmlregistration.h>

// A small C++ model exposed directly to QML. QML reads role names such as
// `title`, `kind`, and `accentColor`; it does not own the media data.
class MediaLibraryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum Role {
        TitleRole = Qt::UserRole + 1,
        KindRole,
        DurationRole,
        AccentColorRole
    };
    Q_ENUM(Role)

    explicit MediaLibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString titleAt(int row) const;

private:
    struct MediaAsset {
        QString title;
        QString kind;
        QString duration;
        QColor accentColor;
    };

    QVector<MediaAsset> assets_;
};
