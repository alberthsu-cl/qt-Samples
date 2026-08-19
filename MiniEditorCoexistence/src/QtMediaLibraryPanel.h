#pragma once

#include <QWidget>

class MediaAssetModel;
class QListView;

// The Phase 1 replacement for MediaLibraryPane. It owns only Qt widgets and
// emits an asset index; MFC remains responsible for the selected project data.
class QtMediaLibraryPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit QtMediaLibraryPanel(QWidget *parent = nullptr);

    void setSelectedAssetIndex(int assetIndex);

signals:
    void assetSelected(int assetIndex);

private:
    MediaAssetModel *assetModel_ = nullptr;
    QListView *assetView_ = nullptr;
};
