#include "Db.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

// Append-only: each entry upgrades user_version by 1.
static const QStringList kMigrations = {
    // v1
    "CREATE TABLE settings(key TEXT PRIMARY KEY, value TEXT NOT NULL);"
    "CREATE TABLE connections("
    " id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, mode INTEGER NOT NULL DEFAULT 0,"
    " host TEXT NOT NULL, port INTEGER NOT NULL DEFAULT 3306, user TEXT NOT NULL,"
    " password_enc TEXT NOT NULL DEFAULT '', db TEXT NOT NULL, ssl INTEGER NOT NULL DEFAULT 0,"
    " color TEXT NOT NULL DEFAULT '#0A84FF', ssh_host TEXT NOT NULL DEFAULT '',"
    " ssh_port INTEGER NOT NULL DEFAULT 22, ssh_user TEXT NOT NULL DEFAULT '',"
    " ssh_auth INTEGER NOT NULL DEFAULT 0, ssh_secret_enc TEXT NOT NULL DEFAULT '',"
    " http_url TEXT NOT NULL DEFAULT '', http_token_enc TEXT NOT NULL DEFAULT '',"
    " last_used INTEGER NOT NULL DEFAULT 0);",
    // v2: SSH host key (TOFU), persisted tabs, query history
    "ALTER TABLE connections ADD COLUMN ssh_fp TEXT NOT NULL DEFAULT '';"
    "CREATE TABLE tabs(id INTEGER PRIMARY KEY AUTOINCREMENT, conn_id INTEGER NOT NULL, kind TEXT NOT NULL,"
    " ref TEXT NOT NULL DEFAULT '', extra TEXT NOT NULL DEFAULT '', title TEXT NOT NULL DEFAULT '',"
    " pos INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE query_history(id INTEGER PRIMARY KEY AUTOINCREMENT, conn_id INTEGER NOT NULL,"
    " sql TEXT NOT NULL, ts INTEGER NOT NULL);",
};

bool Db::open(const QString &path)
{
    QSqlDatabase db = QSqlDatabase::contains() ? QSqlDatabase::database() : QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(path);
    if (!db.open()) {
        qWarning() << "db open failed:" << db.lastError().text();
        return false;
    }
    QSqlQuery q(db);
    q.exec("PRAGMA user_version");
    q.next();
    for (int v = q.value(0).toInt(); v < kMigrations.size(); ++v) {
        db.transaction();
        for (const QString &stmt : kMigrations[v].split(';', Qt::SkipEmptyParts)) {
            if (!q.exec(stmt)) {
                qWarning() << "migration" << v + 1 << "failed:" << q.lastError().text();
                db.rollback();
                return false;
            }
        }
        q.exec(QString("PRAGMA user_version=%1").arg(v + 1));
        db.commit();
    }
    return true;
}

QString Db::get(const QString &key, const QString &def)
{
    QSqlQuery q;
    q.prepare("SELECT value FROM settings WHERE key=?");
    q.addBindValue(key);
    return q.exec() && q.next() ? q.value(0).toString() : def;
}

void Db::set(const QString &key, const QString &value)
{
    QSqlQuery q;
    q.prepare("INSERT INTO settings(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value");
    q.addBindValue(key);
    q.addBindValue(value);
    if (!q.exec())
        qWarning() << "setting save failed:" << q.lastError().text();
}
