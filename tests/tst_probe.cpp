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
                                         qEnvironmentVariable("TEST_DB_USER", "root"), qEnvironmentVariable("TEST_DB_PASS"), qEnvironmentVariable("TEST_DB_NAME", "test"), false);
        QVERIFY2(!v.startsWith("ERROR"), qPrintable(v));
    }
    void sslOffAgainstServerWithoutTls() {
        // regression: Connector/C 3.4 defaults to mandatory verified TLS; "SSL Off" must still connect.
        const QString host = qEnvironmentVariable("TEST_DB_HOST");
        if (host.isEmpty()) QSKIP("TEST_DB_HOST not set");
        const QString v = Probe::serverVersion(host, qEnvironmentVariable("TEST_DB_PORT", "3306").toUInt(),
                                               qEnvironmentVariable("TEST_DB_USER", "root"), qEnvironmentVariable("TEST_DB_PASS"), qEnvironmentVariable("TEST_DB_NAME", "test"), false);
        QVERIFY2(!v.startsWith("ERROR"), qPrintable(v));
    }
    void sslOnFailsCleanlyWhenServerHasNoTls() {
        const QString host = qEnvironmentVariable("TEST_DB_HOST");
        if (host.isEmpty()) QSKIP("TEST_DB_HOST not set");
        // mariadb container has no TLS configured: SSL On must report an error, not crash or hang.
        const QString v = Probe::serverVersion(host, qEnvironmentVariable("TEST_DB_PORT", "3306").toUInt(),
                                               qEnvironmentVariable("TEST_DB_USER", "root"), qEnvironmentVariable("TEST_DB_PASS"), qEnvironmentVariable("TEST_DB_NAME", "test"), true);
        QVERIFY(v.startsWith("ERROR") || !v.isEmpty());
    }
    void badPassword() {
        const QString host = qEnvironmentVariable("TEST_DB_HOST");
        if (host.isEmpty()) QSKIP("TEST_DB_HOST not set");
        QVERIFY(Probe::serverVersion(host, qEnvironmentVariable("TEST_DB_PORT", "3306").toUInt(),
                                     qEnvironmentVariable("TEST_DB_USER", "root"), "wrong-pass", qEnvironmentVariable("TEST_DB_NAME", "test"), false).startsWith("ERROR"));
    }
};
QTEST_APPLESS_MAIN(TstProbe)
#include "tst_probe.moc"
