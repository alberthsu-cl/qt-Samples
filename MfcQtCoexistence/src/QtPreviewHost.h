#pragma once

#include <afxwin.h>
#include <atlimage.h>

#include <memory>
#include <functional>

class QtEffectPreviewPanel;
class QImage;

// The explicit native-window boundary between the MFC frame and the persistent
// Qt panel. Qt owns its panel; MFC owns the surrounding main frame.
class QtPreviewHost final
{
public:
    using DisplayModeChangedHandler = std::function<void(bool showingProcessedImage)>;

    QtPreviewHost();
    ~QtPreviewHost();

    QtPreviewHost(const QtPreviewHost &) = delete;
    QtPreviewHost &operator=(const QtPreviewHost &) = delete;

    bool create(void *mfcParentWindowHandle);
    void resize(const CRect &bounds);
    void setImages(const CImage &originalImage, const CImage &processedImage);
    void setShowingProcessedImage(bool showingProcessedImage);
    void setDisplayModeChangedHandler(DisplayModeChangedHandler handler);

private:
    static QImage convertToQImage(const CImage &image);

    std::unique_ptr<QtEffectPreviewPanel> previewPanel_;
    DisplayModeChangedHandler displayModeChangedHandler_;
};
