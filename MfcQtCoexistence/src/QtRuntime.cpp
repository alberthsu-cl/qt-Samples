#include "QtRuntime.h"

#include <QApplication>

QtRuntime::QtRuntime()
    : application_(std::make_unique<QApplication>(argumentCount_, arguments_))
{
    // The MFC main frame is the application's primary window. Closing the
    // temporary Qt settings dialog must never request application shutdown.
    application_->setQuitOnLastWindowClosed(false);
}

QtRuntime::~QtRuntime() = default;
