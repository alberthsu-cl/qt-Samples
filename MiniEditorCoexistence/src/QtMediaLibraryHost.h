#pragma once

#include <afxwin.h>

#include <functional>
#include <memory>

class QtMediaLibraryPanel;
class MediaLibrary;

// Explicit HWND bridge: MFC owns the main frame; Qt owns this child panel.
class QtMediaLibraryHost final
{
public:
    using AssetSelectedHandler = std::function<void(int assetIndex)>;

    QtMediaLibraryHost();
    ~QtMediaLibraryHost();

    QtMediaLibraryHost(const QtMediaLibraryHost &) = delete;
    QtMediaLibraryHost &operator=(const QtMediaLibraryHost &) = delete;

    bool create(void *mfcParentWindowHandle, const MediaLibrary &mediaLibrary);
    void resize(const CRect &bounds);
    void setSelectedAssetIndex(int assetIndex);
    void refreshAssets();
    void setAssetSelectedHandler(AssetSelectedHandler handler);

private:
    std::unique_ptr<QtMediaLibraryPanel> panel_;
    AssetSelectedHandler assetSelectedHandler_;
};
