#pragma once

#include "ProjectState.h"

#include <afxwin.h>

#include <functional>
#include <memory>

class QtTransportPanel;

class QtTransportHost final
{
public:
    using PlaybackCommandHandler = std::function<void(LegacyPlaybackCommand command)>;
    using PlaybackPositionHandler = std::function<void(int frame)>;
    using PlaybackRateHandler = std::function<void(int ratePercent)>;

    QtTransportHost();
    ~QtTransportHost();

    QtTransportHost(const QtTransportHost &) = delete;
    QtTransportHost &operator=(const QtTransportHost &) = delete;

    bool create(void *mfcParentWindowHandle);
    void resize(const CRect &bounds);
    void setPlaybackState(const PlaybackState &state);
    void setPlaybackCommandHandler(PlaybackCommandHandler handler);
    void setPlaybackPositionHandler(PlaybackPositionHandler handler);
    void setPlaybackRateHandler(PlaybackRateHandler handler);

private:
    std::unique_ptr<QtTransportPanel> panel_;
    PlaybackCommandHandler playbackCommandHandler_;
    PlaybackPositionHandler playbackPositionHandler_;
    PlaybackRateHandler playbackRateHandler_;
};
