#pragma once

#include "ProjectState.h"

#include <afxwin.h>

#include <functional>
#include <memory>

class QtTimelineToolbar;

class QtTimelineToolbarHost final
{
public:
    using ViewStateEditedHandler = std::function<void(const TimelineViewState &state)>;
    using FitTimelineHandler = std::function<void()>;
    using SplitClipHandler = std::function<void()>;

    QtTimelineToolbarHost();
    ~QtTimelineToolbarHost();

    bool create(void *mfcParentWindowHandle);
    void resize(const CRect &bounds);
    void setViewState(const TimelineViewState &state);
    void setSplitEnabled(bool isEnabled);
    void setViewStateEditedHandler(ViewStateEditedHandler handler);
    void setFitTimelineHandler(FitTimelineHandler handler);
    void setSplitClipHandler(SplitClipHandler handler);

private:
    std::unique_ptr<QtTimelineToolbar> toolbar_;
    ViewStateEditedHandler viewStateEditedHandler_;
    FitTimelineHandler fitTimelineHandler_;
    SplitClipHandler splitClipHandler_;
};
