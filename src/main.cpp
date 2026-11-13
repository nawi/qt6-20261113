#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include "Probe.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("buildInfo", QVariantMap{
        {"mariadb", Probe::mariadbClientVersion()},
        {"openssl", Probe::opensslVersion()},
        {"ssh", Probe::sshBridgeVersion()},
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("NawiDBMan", "Main");
    return app.exec();
}
