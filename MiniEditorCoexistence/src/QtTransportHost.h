#pragma once

#include "ProjectState.h"

#include <afxwin.h>

#include <functional>
#include <memory>

class QtTransportPanel;

class QtTransportHost final
{
public:
    using PlaybackCommandHandler = std::function<void(PlaybackCommand command)>;
    using PlaybackPositionHandler = std::function<void(int frame)>;

    QtTransportHost();
    ~QtTransportHost();

    QtTransportHost(const QtTransportHost &) = delete;
    QtTransportHost &operator=(const QtTransportHost &) = delete;

    bool create(void *mfcParentWindowHandle);
    void resize(const CRect &bounds);
    void setPlaybackState(const PlaybackState &state);
    void setPlaybackCommandHandler(PlaybackCommandHandler handler);
    void setPlaybackPositionHandler(PlaybackPositionHandler handler);

private:
    std::unique_ptr<QtTransportPanel> panel_;
    PlaybackCommandHandler playbackCommandHandler_;
    PlaybackPositionHandler playbackPositionHandler_;
};
