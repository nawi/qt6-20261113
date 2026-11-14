#include <QtTest>
#include "DbClient.h"

namespace {
QString env(const char *k, const char *def = "") {
    const QString v = qEnvironmentVariable(k);
    return v.isEmpty() ? QString::fromLatin1(def) : v;
}
DbClient::Config cfg(const QString &db = QString()) {
    DbClient::Config c;
    c.host = env("TEST_DB_HOST", "127.0.0.1");
    c.port = quint16(env("TEST_DB_PORT", "3306").toUInt());
    c.user = env("TEST_DB_USER", "root");
    c.password = env("TEST_DB_PASS");
    c.database = db.isEmpty() ? env("TEST_DB_NAME", "test") : db;
    c.connectTimeout = 5;
    c.readTimeout = 20;
    return c;
}
bool available() { return !qEnvironmentVariable("TEST_DB_HOST").isEmpty(); }
}

class TstDbClient : public QObject {
    Q_OBJECT
private:
    DbClient db;
    QString table;

private slots:
    void initTestCase() {
        if (!available()) QSKIP("TEST_DB_HOST not set");
        QString err;
        QVERIFY2(db.open(cfg(), &err), qPrintable(err));
        table = "nawi_t_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        QVERIFY(db.query("DROP TABLE IF EXISTS " + table).ok());
        QVERIFY(db.query("CREATE TABLE " + table +
                         " (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(50) NULL,"
                         " amount DECIMAL(10,2) NOT NULL DEFAULT 0, created DATETIME NULL,"
                         " UNIQUE KEY uq_name(name))").ok());
        QVERIFY(db.query("INSERT INTO " + table + "(name,amount,created) VALUES"
                         " ('alpha',1.25,NOW()),('',2.00,NULL),(NULL,3.00,NULL),"
                         " ('Ünïcode ✓',4.00,NULL),('omega',5.00,NULL)").ok());
    }

    void cleanupTestCase() {
        if (db.isOpen() && !table.isEmpty()) db.query("DROP TABLE IF EXISTS " + table);
    }

    void connectAndVersion() {
        QVERIFY(db.isOpen());
        QVERIFY(!db.serverVersion().isEmpty());
        QVERIFY(db.threadId() > 0);
    }

    void queryColumnsAndRows() {
        QueryResult r = db.query("SELECT id, name, amount, created FROM " + table + " ORDER BY id");
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.columns.size(), 4);
        QCOMPARE(r.columns[0].name, QString("id"));
        QCOMPARE(r.columns[1].name, QString("name"));
        QCOMPARE(r.rows.size(), 5);
        QCOMPARE(r.rows[0][1].toString(), QString("alpha"));
    }

    void nullVsEmptyString() {
        QueryResult r = db.query("SELECT name FROM " + table + " ORDER BY id");
        QVERIFY(r.ok());
        QVERIFY(!r.rows[1][0].isNull());
        QCOMPARE(r.rows[1][0].toString(), QString(""));
        QVERIFY(r.rows[2][0].isNull());
    }

    void utf8mb4RoundTrip() {
        const QString text = QString::fromUtf8("emoji \xF0\x9F\x98\x80 and Ünïcode");
        QVERIFY(db.query("INSERT INTO " + table + "(name,amount) VALUES('" + db.escape(text) + "',9.00)").ok());
        QueryResult r = db.query("SELECT name FROM " + table + " WHERE amount=9.00");
        QVERIFY(r.ok());
        QCOMPARE(r.rows.size(), 1);
        QCOMPARE(r.rows[0][0].toString(), text);
        QVERIFY(db.query("DELETE FROM " + table + " WHERE amount=9.00").ok());
    }

    void truncation() {
        QueryResult r = db.query("SELECT id FROM " + table + " ORDER BY id", 2);
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.rows.size(), 2);
        QVERIFY(r.truncated);
        QueryResult all = db.query("SELECT id FROM " + table + " ORDER BY id", -1);
        QCOMPARE(all.rows.size(), 5);
        QVERIFY(!all.truncated);
    }

    void affectedAndInsertId() {
        QueryResult ins = db.query("INSERT INTO " + table + "(name) VALUES('ins')");
        QVERIFY2(ins.ok(), qPrintable(ins.error));
        QCOMPARE(ins.affected, qint64(1));
        QVERIFY(ins.insertId > 0);
        QueryResult upd = db.query("UPDATE " + table + " SET amount=7 WHERE name='ins'");
        QVERIFY(upd.ok());
        QCOMPARE(upd.affected, qint64(1));
        QVERIFY(db.query("DELETE FROM " + table + " WHERE name='ins'").ok());
    }

    void syntaxError() {
        QueryResult r = db.query("SELEC * FROM nope");
        QVERIFY(!r.ok());
        QCOMPARE(r.errorCode, 1064);
        QVERIFY(!r.error.isEmpty());
    }

    void badPassword() {
        DbClient b;
        QString err;
        auto c = cfg();
        c.password = "definitely-wrong";
        QVERIFY(!b.open(c, &err));
        QVERIFY(err.contains("Access denied", Qt::CaseInsensitive));
    }

    void unknownDatabase() {
        DbClient b;
        QString err;
        QVERIFY(!b.open(cfg("db_that_does_not_exist_xyz"), &err));
        QVERIFY(err.contains("Unknown database", Qt::CaseInsensitive));
    }

    void unreachable() {
        DbClient b;
        QString err;
        auto c = cfg();
        c.host = "127.0.0.1";
        c.port = 1;
        c.connectTimeout = 2;
        QVERIFY(!b.open(c, &err));
        QVERIFY(err.contains("unreachable", Qt::CaseInsensitive));
    }

    void privilegeErrorMapped() {
        QVERIFY(DbClient::friendlyError(1044, "x").startsWith("Not enough privileges"));
        QVERIFY(DbClient::friendlyError(1142, "x").startsWith("Not enough privileges"));
        QVERIFY(DbClient::isPrivilegeError(1370));
        QCOMPARE(DbClient::friendlyError(1064, "syntax"), QString("syntax"));
    }

    void quoteIdentifier() {
        QCOMPARE(DbClient::quoteId("user`data"), QString("`user``data`"));
        QCOMPARE(DbClient::quoteId("plain"), QString("`plain`"));
    }

    void testConnectionCounts() {
        const QVariantMap r = DbClient::test(cfg());
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QVERIFY(!r.value("version").toString().isEmpty());
        QVERIFY(r.value("tables").toInt() >= 1);
        QCOMPARE(r.value("warnings").toStringList().size(), 0);
    }

    void testConnectionBadPassword() {
        auto c = cfg();
        c.password = "wrong";
        const QVariantMap r = DbClient::test(c);
        QVERIFY(!r.value("ok").toBool());
        QVERIFY(!r.value("error").toString().isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstDbClient)
#include "tst_dbclient.moc"
