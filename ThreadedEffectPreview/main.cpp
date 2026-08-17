#include "EffectType.h"
#include "PreviewWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    // Register the enum because it travels across a queued signal between the
    // UI thread and the FrameProcessor worker thread.
    qRegisterMetaType<EffectType>("EffectType");

    PreviewWindow window;
    window.show();

    return application.exec();
}
