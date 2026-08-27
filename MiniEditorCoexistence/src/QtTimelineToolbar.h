#pragma once

#include "ProjectState.h"

#include <QWidget>

class QLabel;
class QSlider;
class QToolButton;

// Qt owns only standard timeline controls. It emits user intent and never
// owns timeline state or the custom MFC drawing surface.
class QtTimelineToolbar final : public QWidget
{
    Q_OBJECT

public:
    explicit QtTimelineToolbar(QWidget *parent = nullptr);
    void setViewState(const TimelineViewState &state);
    void setSplitEnabled(bool isEnabled);

signals:
    void viewStateEdited(int zoomPercent, bool isAudioTrackVisible,
                         bool isRippleEditingEnabled);
    void fitTimelineRequested();
    void splitClipRequested();

private:
    QSlider *zoomSlider_ = nullptr;
    QLabel *zoomLabel_ = nullptr;
    QToolButton *fitButton_ = nullptr;
    QToolButton *splitButton_ = nullptr;
    QToolButton *rippleButton_ = nullptr;
    bool isAudioTrackVisible_ = true;
};
