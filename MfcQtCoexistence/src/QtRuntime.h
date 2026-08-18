#pragma once

#include <memory>

class QApplication;

// MFC still owns the main Win32 message loop. This small object creates the
// one QApplication required by all Qt Widgets used inside the MFC process.
class QtRuntime final
{
public:
    QtRuntime();
    ~QtRuntime();

    QtRuntime(const QtRuntime &) = delete;
    QtRuntime &operator=(const QtRuntime &) = delete;

private:
    // QApplication receives argc/argv by reference, so these values live for
    // exactly as long as QApplication does.
    char applicationName_[32] = "MfcQtCoexistence";
    char *arguments_[1] = { applicationName_ };
    int argumentCount_ = 1;
    std::unique_ptr<QApplication> application_;
};
