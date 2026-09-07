#include "EditorViewModel.h"

EditorViewModel::EditorViewModel(QObject *parent)
    : QObject(parent)
    , mediaLibrary_(this)
{
}

MediaLibraryModel *EditorViewModel::mediaLibrary()
{
    return &mediaLibrary_;
}

int EditorViewModel::selectedAssetIndex() const
{
    return selectedAssetIndex_;
}

QString EditorViewModel::selectedAssetTitle() const
{
    return mediaLibrary_.titleAt(selectedAssetIndex_);
}

void EditorViewModel::selectAsset(int index)
{
    if (index < 0 || index >= mediaLibrary_.rowCount() || index == selectedAssetIndex_)
        return;

    selectedAssetIndex_ = index;
    emit selectedAssetChanged();
}
