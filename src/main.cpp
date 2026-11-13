#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include "AppSettings.h"
#include "ConnectionStore.h"
#include "Db.h"
#include "Platform.h"
#include "Probe.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("NawiDBMan");
    app.setApplicationVersion(APP_VERSION);
    QQuickStyle::setStyle("Basic");

    const QString dir = qEnvironmentVariable("NAWI_DATA", QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QDir().mkpath(dir);
    if (!Db::open(dir + "/nawidbman.db"))
        return 1;

#ifndef Q_OS_ANDROID
    // Desktop preview helpers (never compiled into the APK).
    if (qEnvironmentVariableIsSet("NAWI_THEME")) Db::set("theme", qEnvironmentVariable("NAWI_THEME"));
    if (qEnvironmentVariableIsSet("NAWI_ONBOARDED")) Db::set("onboarded", qEnvironmentVariable("NAWI_ONBOARDED"));
#endif

    AppSettings settings;
    ConnectionStore connections;
    Platform platform;

#ifndef Q_OS_ANDROID
    if (qEnvironmentVariableIsSet("NAWI_SEED") && connections.rowCount() == 0) {
        connections.save({{"name", "Production"}, {"mode", 1}, {"host", "10.0.0.5"}, {"port", 3306}, {"user", "app"},
                          {"database", "shop"}, {"color", "#FF3B30"}, {"sshHost", "bastion.example.com"}, {"sshPort", 22},
                          {"sshUser", "deploy"}, {"sshAuth", 0}, {"sshSecret", "x"}});
        connections.save({{"name", "Staging (HTTP agent)"}, {"mode", 2}, {"host", "localhost"}, {"port", 3306}, {"user", "stg"},
                          {"database", "shop_stg"}, {"color", "#FF9500"}, {"httpUrl", "https://stg.example.com/tunnel.php"},
                          {"httpToken", "t"}});
        connections.save({{"name", "Local MariaDB"}, {"mode", 0}, {"host", "192.168.1.20"}, {"port", 3306}, {"user", "root"},
                          {"database", "test"}, {"color", "#34C759"}, {"ssl", true}});
    }
#endif

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appSettings", &settings);
    engine.rootContext()->setContextProperty("connections", &connections);
    engine.rootContext()->setContextProperty("platform", &platform);
    engine.rootContext()->setContextProperty("buildInfo", QVariantMap{
        {"version", APP_VERSION},
        {"mariadb", Probe::mariadbClientVersion()},
        {"openssl", Probe::opensslVersion()},
    });
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [](const QList<QQmlError> &w) {
        for (const auto &e : w) qWarning().noquote() << e.toString();
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { qCritical() << "QML load failed"; QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("NawiDBMan", "Main");

#ifndef Q_OS_ANDROID
    if (qEnvironmentVariableIsSet("NAWI_SHOT")) {
        QTimer::singleShot(1500, &app, [&] {
            auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
            if (w) w->grabWindow().save(qEnvironmentVariable("NAWI_SHOT"));
            QCoreApplication::quit();
        });
    }
#endif
    return app.exec();
}
