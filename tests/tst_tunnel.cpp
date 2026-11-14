#include <QtTest>
#include <QFile>
#include "DbClient.h"
#include "SshTunnel.h"

extern "C" unsigned long long ssh_bridge_connect(const char *, unsigned short, const char *, int,
                                                 const char *, const char *, const char *, unsigned short,
                                                 int, unsigned short *, char *, int, int *, char *, int);
extern "C" void ssh_bridge_disconnect(unsigned long long);

class TstTunnel : public QObject {
    Q_OBJECT
    QString key;
    QString host;
    quint16 port = 0;
    QString user;

    unsigned long long connectTunnel(const QString &fp, int *rc, QString *outFp, QString *err)
    {
        QByteArray h = host.toUtf8(), u = user.toUtf8(), k = key.toUtf8(), f = fp.toUtf8();
        QByteArray d = "127.0.0.1";
        QByteArray e(512, '\0'), o(256, '\0');
        unsigned short local = 0;
        int code = 0;
        const auto handle = ssh_bridge_connect(h.constData(), port, u.constData(), 1, k.constData(),
                                                f.constData(), d.constData(), quint16(qEnvironmentVariable("TEST_DB_PORT", "3306").toUInt()), 10, &local, o.data(),
                                                o.size(), &code, e.data(), e.size());
        if (rc) *rc = code;
        if (outFp) *outFp = QString::fromUtf8(o.constData());
        if (err) *err = QString::fromUtf8(e.constData());
        if (handle) { m_ports[handle] = local; }
        return handle;
    }

public:
    QHash<unsigned long long, quint16> m_ports;

private slots:
    void initTestCase() {
        host = qEnvironmentVariable("TEST_SSH_HOST");
        port = quint16(qEnvironmentVariable("TEST_SSH_PORT", "22").toUInt());
        user = qEnvironmentVariable("TEST_SSH_USER");
        const QString keyPath = qEnvironmentVariable("TEST_SSH_KEY");
        if (host.isEmpty() || user.isEmpty() || keyPath.isEmpty()) QSKIP("TEST_SSH_* not set");
        QFile f(keyPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        key = QString::fromUtf8(f.readAll());
    }
    void cleanupTestCase() {
        for (auto h : m_ports.keys()) ssh_bridge_disconnect(h);
    }

    void tofuThenTrust() {
        int rc = 0;
        QString fp, err;
        auto h = connectTunnel({}, &rc, &fp, &err);
        QCOMPARE(h, 0ull);
        QCOMPARE(rc, 3);             // host key unknown
        QVERIFY(fp.startsWith("SHA256:"));

        h = connectTunnel(fp, &rc, &fp, &err);
        QVERIFY2(h != 0, qPrintable(err));
        QCOMPARE(rc, 0);
        QVERIFY(m_ports.value(h) > 0);
    }

    void wrongFingerprintRejected() {
        int rc = 0;
        QString fp, err;
        const auto h = connectTunnel("SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", &rc, &fp, &err);
        QCOMPARE(h, 0ull);
        QCOMPARE(rc, 4);
    }

    void tunnelCarriesDatabase() {
        int rc = 0;
        QString fp, err;
        auto h = connectTunnel({}, &rc, &fp, &err);
        QCOMPARE(rc, 3);
        h = connectTunnel(fp, &rc, &fp, &err);
        QVERIFY(h);
        DbClient::Config c;
        c.host = "127.0.0.1";
        c.port = m_ports.value(h);
        c.user = qEnvironmentVariable("TEST_DB_USER");
        c.password = qEnvironmentVariable("TEST_DB_PASS");
        c.database = qEnvironmentVariable("TEST_DB_NAME", "test");
        c.connectTimeout = 5;
        DbClient db;
        QString e;
        QVERIFY2(db.open(c, &e), qPrintable(e));
        const QueryResult r = db.query("SELECT 1", -1);
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.rows.first().first().toString(), QString("1"));
    }
};

QTEST_GUILESS_MAIN(TstTunnel)
#include "tst_tunnel.moc"
