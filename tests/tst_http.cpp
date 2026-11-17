#include <QtTest>
#include "DbClient.h"

namespace {
QString env(const char *k, const char *def = "") {
    const QString v = qEnvironmentVariable(k);
    return v.isEmpty() ? QString::fromLatin1(def) : v;
}
DbClient::Config cfg() {
    DbClient::Config c;
    c.httpUrl = env("TEST_HTTP_URL");
    c.httpToken = env("TEST_HTTP_TOKEN");
    c.host = "127.0.0.1";   // as seen from the agent
    c.port = quint16(env("TEST_DB_PORT", "3306").toUInt());
    c.user = env("TEST_DB_USER", "root");
    c.password = env("TEST_DB_PASS");
    c.database = env("TEST_DB_NAME", "test");
    c.readTimeout = 20;
    c.httpEncrypt = true;   // exercise the AES-256-GCM envelope (agent accepts both)
    return c;
}
}

class TstHttp : public QObject {
    Q_OBJECT
    DbClient db;
private slots:
    void initTestCase() {
        if (qEnvironmentVariable("TEST_HTTP_URL").isEmpty()) QSKIP("TEST_HTTP_URL not set");
        QString err;
        QVERIFY2(db.open(cfg(), &err), qPrintable(err));
    }
    void pingGivesVersion() { QVERIFY(!db.serverVersion().isEmpty()); }
    void nullVsEmptyAndUtf8() {
        QueryResult r = db.query("SELECT NULL, '', 'Ünï ✓', 42", -1);
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.columns.size(), 4);
        QVERIFY(r.rows[0][0].isNull());
        QVERIFY(!r.rows[0][1].isNull());
        QCOMPARE(r.rows[0][1].toString(), QString());
        QCOMPARE(r.rows[0][2].toString(), QString::fromUtf8("Ünï ✓"));
        QCOMPARE(r.rows[0][3].toString(), QString("42"));
    }
    void truncation() {
        QueryResult r = db.query("SELECT 1 UNION SELECT 2 UNION SELECT 3", 2);
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.rows.size(), 2);
        QVERIFY(r.truncated);
        r = db.query("SELECT 1 UNION SELECT 2", 5);
        QVERIFY(!r.truncated);
    }
    void writeAffected() {
        QVERIFY(db.query("DROP TABLE IF EXISTS nawi_http_t", -1).ok());
        QVERIFY(db.query("CREATE TABLE nawi_http_t(id INT AUTO_INCREMENT PRIMARY KEY, v INT)", -1).ok());
        QueryResult r = db.query("INSERT INTO nawi_http_t(v) VALUES (1),(2)", -1);
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.affected, qint64(2));
        QVERIFY(r.insertId > 0);
        QVERIFY(db.query("DROP TABLE nawi_http_t", -1).ok());
    }
    void syntaxError() {
        QueryResult r = db.query("SELEC 1", -1);
        QVERIFY(!r.ok());
        QCOMPARE(r.errorCode, 1064);
    }
    void badPassword() {
        DbClient::Config c = cfg();
        c.password = "definitely-wrong";
        DbClient d;
        QString err;
        QVERIFY(!d.open(c, &err));
        QVERIFY2(err.contains("Access denied"), qPrintable(err));
    }
    void badToken() {
        DbClient::Config c = cfg();
        c.httpToken = "nope";
        DbClient d;
        QString err;
        QVERIFY(!d.open(c, &err));
        QVERIFY2(err.contains("401"), qPrintable(err));
    }
    void remoteHostRejected() {
        DbClient::Config c = cfg();
        c.host = "example.com";
        DbClient d;
        QString err;
        QVERIFY(!d.open(c, &err));
        QVERIFY2(err.contains("403"), qPrintable(err));
    }
    void unreachableAgent() {
        DbClient::Config c = cfg();
        c.httpUrl = "http://127.0.0.1:1/tunnel.php";
        DbClient d;
        QString err;
        QVERIFY(!d.open(c, &err));
        QVERIFY2(err.contains("unreachable"), qPrintable(err));
    }
};

QTEST_GUILESS_MAIN(TstHttp)
#include "tst_http.moc"
