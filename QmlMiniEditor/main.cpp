#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QQmlApplicationEngine engine;

    // The QML module URI and file name come from qt_add_qml_module() in CMake.
    // Loading from the module keeps the app independent of the current folder.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &application, [] {
                         QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    engine.loadFromModule("QmlMiniEditor", "Main");
    return application.exec();
}
