#pragma once

#include <QWidget>

class MediaAssetModel;
class QLabel;
class QListView;
class MediaLibrary;
class QSortFilterProxyModel;
class QStackedLayout;

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
    void updateEmptyState();

    MediaAssetModel *assetModel_ = nullptr;
    QSortFilterProxyModel *assetFilterModel_ = nullptr;
    QListView *assetView_ = nullptr;
    QLabel *emptyStateLabel_ = nullptr;
    QStackedLayout *assetContentLayout_ = nullptr;
};
