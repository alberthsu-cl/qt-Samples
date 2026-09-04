#pragma once

#include "ProjectState.h"

#include <afxwin.h>

#include <functional>
#include <memory>

class QtPreviewPanel;
class QVideoSink;
class QImage;

// Embeds the Qt preview widget in the still-MFC-owned frame during migration.
class QtPreviewHost final
{
public:
    QtPreviewHost();
    ~QtPreviewHost();

    QtPreviewHost(const QtPreviewHost &) = delete;
    QtPreviewHost &operator=(const QtPreviewHost &) = delete;

    bool create(void *mfcParentWindowHandle);
    void resize(const CRect &bounds);
    void setPreviewState(const PreviewState &state);
    void setPlaybackState(const PlaybackState &state);
    void setFallbackImage(const QImage &image);
    QVideoSink *videoSink() const;
    void setDecodedVideoVisible(bool visible);

    // M5-05: the routed engine's own surface inside this same panel. Separate
    // sink, no redirection of the legacy one (decision B).
    QVideoSink *engineVideoSink() const;
    void setEnginePresentationActive(bool active);
    void setEngineFrameCommittedHandler(std::function<void()> handler);

private:
    std::unique_ptr<QtPreviewPanel> panel_;
};
