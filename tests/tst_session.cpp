#include <QtTest>
#include <QEventLoop>
#include <QTimer>
#include "DbSession.h"

namespace {
QString env(const char *k, const char *def = "") {
    const QString v = qEnvironmentVariable(k);
    return v.isEmpty() ? QString::fromLatin1(def) : v;
}
bool available() { return !qEnvironmentVariable("TEST_DB_HOST").isEmpty(); }
QVariantMap conn() {
    return {{"id", 999}, {"name", "test"}, {"host", env("TEST_DB_HOST", "127.0.0.1")},
            {"port", env("TEST_DB_PORT", "3306").toInt()}, {"user", env("TEST_DB_USER", "root")},
            {"password", env("TEST_DB_PASS")}, {"database", env("TEST_DB_NAME", "test")},
            {"mode", 0}, {"ssl", false}};
}
QVariantMap run(DbSession &s, const std::function<void()> &f, const QString &kind)
{
    QVariantMap out;
    bool done = false;
    auto c = QObject::connect(&s, &DbSession::finished, &s, [&](const QString &k, const QVariantMap &r) {
        if (k != kind) return;
        out = r;
        done = true;
    });
    f();
    if (done) { QObject::disconnect(c); return out; }
    QEventLoop loop;
    QTimer t;
    t.setSingleShot(true);
    QObject::connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&s, &DbSession::finished, &loop, [&](const QString &k, const QVariantMap &) {
        if (k == kind) loop.quit();
    });
    t.start(15000);
    loop.exec();
    QObject::disconnect(c);
    if (!done) return {{"ok", false}, {"error", "timeout waiting for " + kind}};
    return out;
}
}

class TstSession : public QObject {
    Q_OBJECT
private:
    DbSession *s = nullptr;
    QString table;

private slots:
    void initTestCase() {
        if (!available()) QSKIP("TEST_DB_HOST not set");
        s = new DbSession(this);
        const QVariantMap r = run(*s, [this] {
            s->open(conn(), env("TEST_DB_HOST"), quint16(env("TEST_DB_PORT", "3306").toUInt()));
        }, "open");
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        table = "nawi_s_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        const QString setup = "CREATE TABLE " + table +
            " (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(60) NULL, v DECIMAL(8,2) NOT NULL DEFAULT 0);"
            "INSERT INTO " + table + "(name,v) VALUES ('a',1.00),('b',2.00),(NULL,3.00);"
            "CREATE OR REPLACE VIEW v_" + table + " AS SELECT id,name FROM " + table;
        for (const QString &stmt : setup.split(';', Qt::SkipEmptyParts)) {
            const QVariantMap q = run(*s, [this, stmt] { s->runQuery(stmt, -1); }, "query");
            QVERIFY2(q.value("ok").toBool(), qPrintable(q.value("error").toString()));
        }
    }
    void cleanupTestCase() {
        // The editor guard blocks DROP by design, so clean up with a direct client.
        if (!table.isEmpty()) {
            DbClient db;
            DbClient::Config c;
            c.host = env("TEST_DB_HOST");
            c.port = quint16(env("TEST_DB_PORT", "3306").toUInt());
            c.user = env("TEST_DB_USER", "root");
            c.password = env("TEST_DB_PASS");
            c.database = env("TEST_DB_NAME", "test");
            if (db.open(c)) {
                db.query("DROP VIEW IF EXISTS v_" + table, -1);
                db.query("DROP TABLE IF EXISTS " + table, -1);
            }
        }
        delete s;
    }

    void objects() {
        const QVariantMap r = run(*s, [this] { s->loadObjects(); }, "objects");
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QStringList names;
        for (const QVariant &o : r.value("objects").toList()) names << o.toMap().value("name").toString();
        QVERIFY(names.contains(table));
        QVERIFY(names.contains("v_" + table));
    }

    void tablePageAndPk() {
        const QVariantMap r = run(*s, [this] { s->loadTable(table, 0, 2, {}); }, "table");
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QCOMPARE(r.value("rows").toList().size(), 2);
        QVERIFY(r.value("hasMore").toBool());
        QCOMPARE(r.value("pk").toStringList(), QStringList{"id"});
        QVERIFY(!r.value("readOnly").toBool());
        const QVariantMap next = run(*s, [this] { s->loadTable(table, 2, 2, {}); }, "table");
        QCOMPARE(next.value("rows").toList().size(), 1);
        QVERIFY(!next.value("hasMore").toBool());
    }

    void filterAndInjection() {
        const QVariantList filters{{QVariantMap{{"column", "v"}, {"op", ">"}, {"value", "1.5"}}}};
        const QVariantMap r = run(*s, [this, filters] { s->loadTable(table, 0, 50, filters); }, "table");
        QVERIFY(r.value("ok").toBool());
        QCOMPARE(r.value("rows").toList().size(), 2);
        // Value is escaped, so this is a literal and matches nothing.
        const QVariantList evil{{QVariantMap{{"column", "name"}, {"op", "="}, {"value", "' OR 1=1 --"}}}};
        const QVariantMap e = run(*s, [this, evil] { s->loadTable(table, 0, 50, evil); }, "table");
        QVERIFY(e.value("ok").toBool());
        QCOMPARE(e.value("rows").toList().size(), 0);
    }

    void sortAndSearch() {
        QVariantMap r = run(*s, [this] { s->loadTable(table, 0, 50, {}, "`v` DESC"); }, "table");
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QCOMPARE(r.value("rows").toList()[0].toList()[2].toString(), QString("3.00"));
        r = run(*s, [this] { s->loadTable(table, 0, 50, {}, {}, "b", {"id", "name", "v"}); }, "table");
        QVERIFY(r.value("ok").toBool());
        QCOMPARE(r.value("rows").toList().size(), 1);
        // LIKE wildcards in the search text are literal
        r = run(*s, [this] { s->loadTable(table, 0, 50, {}, {}, "%", {"name"}); }, "table");
        QCOMPARE(r.value("rows").toList().size(), 0);
    }

    void editInsertDelete() {
        const QVariantMap page = run(*s, [this] { s->loadTable(table, 0, 50, {}); }, "table");
        const QVariantList rows = page.value("rows").toList();
        const int id = rows[0].toList()[0].toInt();
        QVariantMap pk{{"id", id}};
        QVariantMap w = run(*s, [this, pk] { s->updateCell(table, "name", "renamed", false, pk); }, "write");
        QVERIFY2(w.value("ok").toBool(), qPrintable(w.value("error").toString()));
        const QVariantMap check = run(*s, [this, id] { s->runQuery("SELECT name FROM " + table + " WHERE id=" + QString::number(id), -1); }, "query");
        QVERIFY(check.value("ok").toBool());

        w = run(*s, [this, pk] { s->updateCell(table, "name", "", true, pk); }, "write");
        QVERIFY(w.value("ok").toBool());

        QVariantMap values{{"name", "inserted"}, {"v", "7.50"}};
        w = run(*s, [this, values] { s->insertRow(table, values); }, "write");
        QVERIFY2(w.value("ok").toBool(), qPrintable(w.value("error").toString()));
        QVERIFY(w.value("insertId").toLongLong() > 0);

        w = run(*s, [this] { s->deleteRow(table, {{"id", 1}}); }, "write");
        QVERIFY(w.value("ok").toBool());
        QCOMPARE(w.value("affected").toLongLong(), qint64(1));
    }

    void design() {
        const QVariantMap r = run(*s, [this] { s->loadDesign("table", table); }, "design");
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QVERIFY(r.value("fields").toMap().value("ok").toBool());
        QVERIFY(r.value("indexes").toMap().value("ok").toBool());
        const QVariantMap ddl = r.value("ddl").toMap();
        QVERIFY(ddl.value("ok").toBool());
        bool found = false;
        for (const QVariant &row : ddl.value("rows").toList())
            for (const QVariant &v : row.toList())
                if (v.toString().contains("CREATE TABLE")) found = true;
        QVERIFY(found);
    }

    void queryLimitAndErrors() {
        const QVariantMap r = run(*s, [this] { s->runQuery("SELECT * FROM " + table, 1); }, "query");
        QVERIFY(r.value("ok").toBool());
        QCOMPARE(r.value("rows").toList().size(), 1);
        QVERIFY(r.value("truncated").toBool());
        const QVariantMap bad = run(*s, [this] { s->runQuery("SELEC 1", -1); }, "query");
        QVERIFY(!bad.value("ok").toBool());
        const QVariantMap drop = run(*s, [this] { s->runQuery("DROP TABLE " + table, -1); }, "query");
        QVERIFY(!drop.value("ok").toBool());
        const QVariantMap del = run(*s, [this] { s->runQuery("DELETE FROM " + table, -1); }, "query");
        QVERIFY(!del.value("ok").toBool());
    }
};

QTEST_MAIN(TstSession)
#include "tst_session.moc"
