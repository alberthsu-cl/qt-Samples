#include "QtMediaLibraryHost.h"

#include "QtMediaLibraryPanel.h"

#include <QObject>

#include <utility>

QtMediaLibraryHost::QtMediaLibraryHost() = default;
QtMediaLibraryHost::~QtMediaLibraryHost() = default;

bool QtMediaLibraryHost::create(void *mfcParentWindowHandle, const MediaLibrary &mediaLibrary)
{
    panel_ = std::make_unique<QtMediaLibraryPanel>(mediaLibrary);
    panel_->setWindowFlag(Qt::FramelessWindowHint, true);
    panel_->setAttribute(Qt::WA_NativeWindow);

    const HWND qtWindowHandle = reinterpret_cast<HWND>(panel_->winId());
    const HWND mfcWindowHandle = static_cast<HWND>(mfcParentWindowHandle);
    if (qtWindowHandle == nullptr || mfcWindowHandle == nullptr)
        return false;

    ::SetParent(qtWindowHandle, mfcWindowHandle);
    const LONG_PTR style = ::GetWindowLongPtr(qtWindowHandle, GWL_STYLE);
    const LONG_PTR frameStyles = WS_POPUP | WS_CAPTION | WS_THICKFRAME |
                                WS_BORDER | WS_DLGFRAME;
    ::SetWindowLongPtr(qtWindowHandle, GWL_STYLE,
                       (style & ~frameStyles) | WS_CHILD | WS_CLIPSIBLINGS);
    ::SetWindowPos(qtWindowHandle, nullptr, 0, 0, 1, 1,
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    QObject::connect(panel_.get(), &QtMediaLibraryPanel::assetSelected,
                     panel_.get(),
                     [this](int assetIndex) {
                         if (assetSelectedHandler_)
                             assetSelectedHandler_(assetIndex);
                     });

    panel_->show();
    return true;
}

void QtMediaLibraryHost::refreshAssets()
{
    if (panel_)
        panel_->refreshAssets();
}

void QtMediaLibraryHost::resize(const CRect &bounds)
{
    if (!panel_)
        return;

    const HWND qtWindowHandle = reinterpret_cast<HWND>(panel_->winId());
    ::SetWindowPos(qtWindowHandle, nullptr,
                   bounds.left, bounds.top, bounds.Width(), bounds.Height(),
                   SWP_NOZORDER | SWP_NOACTIVATE);
}

void QtMediaLibraryHost::setSelectedAssetIndex(int assetIndex)
{
    if (panel_)
        panel_->setSelectedAssetIndex(assetIndex);
}

void QtMediaLibraryHost::setAssetSelectedHandler(AssetSelectedHandler handler)
{
    assetSelectedHandler_ = std::move(handler);
}
