#pragma once

#include <memory>

class QApplication;

// MFC keeps the application loop. This creates the one QApplication required
// by the embedded Qt Widgets.
class QtRuntime final
{
public:
    QtRuntime();
    ~QtRuntime();

    QtRuntime(const QtRuntime &) = delete;
    QtRuntime &operator=(const QtRuntime &) = delete;

private:
    char applicationName_[32] = "MiniEditorCoexistence";
    char *arguments_[1] = { applicationName_ };
    int argumentCount_ = 1;
    std::unique_ptr<QApplication> application_;
};
