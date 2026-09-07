#pragma once

#include "MediaLibraryModel.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>

// This is the C++ side of the first QML lesson. It owns editor selection
// state; QML reads properties and asks for changes through invokable methods.
class EditorViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(MediaLibraryModel *mediaLibrary READ mediaLibrary CONSTANT)
    Q_PROPERTY(int selectedAssetIndex READ selectedAssetIndex NOTIFY selectedAssetChanged)
    Q_PROPERTY(QString selectedAssetTitle READ selectedAssetTitle NOTIFY selectedAssetChanged)

public:
    explicit EditorViewModel(QObject *parent = nullptr);

    MediaLibraryModel *mediaLibrary();
    int selectedAssetIndex() const;
    QString selectedAssetTitle() const;

    Q_INVOKABLE void selectAsset(int index);

signals:
    void selectedAssetChanged();

private:
    MediaLibraryModel mediaLibrary_;
    int selectedAssetIndex_ = 0;
};
