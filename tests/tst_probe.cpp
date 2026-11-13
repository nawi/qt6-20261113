#include <QtTest>
#include "Probe.h"

class TstProbe : public QObject {
    Q_OBJECT
private slots:
    void versions() {
        QVERIFY(!Probe::mariadbClientVersion().isEmpty());
        QVERIFY(Probe::opensslVersion().contains("OpenSSL"));
        QVERIFY(Probe::sshBridgeVersion().endsWith(" ok"));
    }
    void server() {
        const QString host = qEnvironmentVariable("TEST_DB_HOST");
        if (host.isEmpty()) QSKIP("TEST_DB_HOST not set");
        QString v = Probe::serverVersion(host, qEnvironmentVariable("TEST_DB_PORT", "3306").toUInt(),
                                         "root", qEnvironmentVariable("TEST_DB_PASS"), "test", false);
        QVERIFY2(!v.startsWith("ERROR"), qPrintable(v));
    }
    void badPassword() {
        const QString host = qEnvironmentVariable("TEST_DB_HOST");
        if (host.isEmpty()) QSKIP("TEST_DB_HOST not set");
        QVERIFY(Probe::serverVersion(host, qEnvironmentVariable("TEST_DB_PORT", "3306").toUInt(),
                                     "root", "wrong-pass", "test", false).startsWith("ERROR"));
    }
};
QTEST_APPLESS_MAIN(TstProbe)
#include "tst_probe.moc"
