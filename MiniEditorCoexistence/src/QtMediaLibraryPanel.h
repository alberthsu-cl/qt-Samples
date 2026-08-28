#pragma once

#include <QWidget>

class MediaAssetModel;
class QListView;
class MediaLibrary;
class QSortFilterProxyModel;

// The Phase 1 replacement for MfcMediaLibraryPane. It owns only Qt widgets and
// emits an asset index; MFC remains responsible for the selected project data.
class QtMediaLibraryPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit QtMediaLibraryPanel(const MediaLibrary &mediaLibrary, QWidget *parent = nullptr);

    void setSelectedAssetIndex(int assetIndex);
    void refreshAssets();
    void clearSelection();

signals:
    void assetSelected(int assetIndex);
    void importRequested();
    void removeRequested(int assetIndex, int assetId);

private:
    MediaAssetModel *assetModel_ = nullptr;
    QSortFilterProxyModel *assetFilterModel_ = nullptr;
    QListView *assetView_ = nullptr;
};
