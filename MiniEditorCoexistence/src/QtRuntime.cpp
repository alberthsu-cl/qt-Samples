#include "QtRuntime.h"

#include <QApplication>

QtRuntime::QtRuntime()
    : application_(std::make_unique<QApplication>(argumentCount_, arguments_))
{
    application_->setQuitOnLastWindowClosed(false);
}

QtRuntime::~QtRuntime() = default;
