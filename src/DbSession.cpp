#include "DbSession.h"
#include "Db.h"
#include "DbClient.h"
#include "Sql.h"
#include <QThread>
#include <QtConcurrent>

namespace {
QVariantMap errorMap(const QString &error, int code = -1)
{
    return {{"ok", false}, {"error", error}, {"errorCode", code},
            {"privilege", DbClient::isPrivilegeError(code)}};
}

QVariantMap resultMap(const QueryResult &r)
{
    if (!r.ok()) return errorMap(r.error, r.errorCode);
    QVariantList columns;
    QStringList names;
    for (const ResultColumn &c : r.columns) {
        columns << QVariantMap{{"name", c.name}, {"type", c.type}, {"binary", c.binary}};
        names << c.name;
    }
    QVariantList rows;
    rows.reserve(r.rows.size());
    for (const QVariantList &row : r.rows) rows << QVariant(row);
    return {{"ok", true}, {"columns", columns}, {"columnNames", names}, {"rows", rows},
            {"affected", r.affected}, {"insertId", r.insertId}, {"truncated", r.truncated},
            {"elapsedMs", r.elapsedMs}};
}

QVariantMap withKind(const QString &kind, QVariantMap m)
{
    m["kind"] = kind;
    return m;
}

QString pkWhere(DbClient &db, const QVariantMap &pk)
{
    QStringList parts;
    for (auto it = pk.begin(); it != pk.end(); ++it) {
        const QString col = it.key();
        if (it.value().isNull() || !it.value().isValid()) {
            parts << Sql::quoteId(col) + " IS NULL";
        } else {
            parts << Sql::quoteId(col) + "='"
                         + db.escape(it.value().toString()) + '\'';
        }
    }
    return parts.join(" AND ");
}

QStringList loadPrimaryKey(DbClient &db, const QString &database, const QString &table, QString *error)
{
    QueryResult r = db.query("SHOW KEYS FROM " + Sql::qualified(database, table)
                                 + " WHERE Key_name='PRIMARY'", -1);
    if (!r.ok()) {
        if (error) *error = r.error;
        return {};
    }
    QStringList pk;
    // Columns: Table, Non_unique, Key_name, Seq_in_index, Column_name, ...
    for (const QVariantList &row : r.rows) {
        if (row.size() > 4 && !row[4].isNull()) pk << row[4].toString();
    }
    return pk;
}
}

DbSession::DbSession(QObject *parent) : QObject(parent)
{
    m_watcher = new QFutureWatcher<QVariantMap>(this);
    connect(m_watcher, &QFutureWatcher<QVariantMap>::finished, this, [this] {
        const QVariantMap r = m_watcher->result();
        m_busy = false;
        emit busyChanged();
        deliver(r.value("kind").toString(), r);
        pump();
    });
}

DbSession::~DbSession()
{
    m_queue.clear();
    if (m_watcher->isRunning()) {
        const unsigned id = unsigned(m_tid.loadAcquire());
        if (id) {
            const DbClient::Config cfg = m_cfg;
            QtConcurrent::run([cfg, id] {
                DbClient killer;
                if (killer.open(cfg)) killer.query("KILL QUERY " + QString::number(id), -1);
            });
        }
        m_watcher->waitForFinished();
    }
    m_client.close();
}

void DbSession::buildConfig(const QVariantMap &conn, const QString &host, int port)
{
    m_conn = conn;
    m_host = host;
    m_port = port;
    m_cfg.host = host;
    m_cfg.port = quint16(port);
    m_cfg.user = conn.value("user").toString().trimmed();
    m_cfg.password = conn.value("password").toString();
    m_cfg.database = conn.value("database").toString().trimmed();
    m_cfg.ssl = conn.value("ssl").toBool();
    m_cfg.connectTimeout = 10;
    m_cfg.readTimeout = qBound(5, Db::get("queryTimeout", "300").toInt(), 3600);
}

void DbSession::open(const QVariantMap &conn, const QString &host, int port)
{
    buildConfig(conn, host, port);
    submit("open", [this](DbClient &db) {
        QString err;
        if (!db.open(m_cfg, &err)) return withKind("open", errorMap(err));
        m_tid.storeRelease(int(db.threadId()));
        return withKind("open", QVariantMap{{"ok", true}, {"version", db.serverVersion()},
                                            {"cipher", db.sslCipher()}});
    });
}

void DbSession::close()
{
    m_queue.clear();
    cancel();
}

void DbSession::cancel()
{
    const unsigned id = unsigned(m_tid.loadAcquire());
    if (!m_busy || !id) return;
    const DbClient::Config cfg = m_cfg;
    QtConcurrent::run([cfg, id] {
        DbClient killer;
        if (killer.open(cfg)) killer.query("KILL QUERY " + QString::number(id), -1);
    });
}

QString DbSession::connectionLabel() const
{
    return m_conn.value("name").toString();
}

void DbSession::submit(const QString &kind, std::function<QVariantMap(DbClient &)> job)
{
    m_queue.enqueue({kind, std::move(job)});
    emit busyChanged();
    pump();
}

void DbSession::pump()
{
    if (m_busy || m_queue.isEmpty() || m_watcher->isRunning()) return;
    const auto item = m_queue.dequeue();
    const QString kind = item.first;
    m_busy = true;
    emit busyChanged();
    m_watcher->setFuture(QtConcurrent::run([this, kind, job = item.second] {
        QVariantMap r = job(m_client);
        r["kind"] = kind;
        return r;
    }));
}

void DbSession::deliver(const QString &kind, const QVariantMap &result)
{
    if (kind == "open") {
        const bool ok = result.value("ok").toBool();
        if (ok) {
            m_version = result.value("version").toString();
            m_lastError.clear();
        } else {
            m_lastError = result.value("error").toString();
        }
        if (m_connected != ok) {
            m_connected = ok;
            emit connectedChanged();
        }
    }
    emit finished(kind, result);
}

void DbSession::runQuery(const QString &sql, int limit)
{
    if (const QString guard = Sql::guardWrite(sql); !guard.isEmpty()) {
        const QVariantMap r = withKind("query", errorMap(guard));
        QMetaObject::invokeMethod(this, [this, r] { emit finished("query", r); }, Qt::QueuedConnection);
        return;
    }
    submit("query", [sql, limit](DbClient &db) {
        return withKind("query", resultMap(db.query(sql, limit)));
    });
}

void DbSession::loadObjects()
{
    submit("objects", [this](DbClient &db) {
        QVariantList groups;
        QStringList warnings;
        const QString dbName = m_conn.value("database").toString();

        QueryResult tables = db.query(
            "SELECT TABLE_NAME, TABLE_TYPE FROM information_schema.TABLES"
            " WHERE TABLE_SCHEMA=DATABASE() ORDER BY TABLE_NAME", -1);
        if (!tables.ok()) {
            const QString original = tables.error;
            tables = db.query("SHOW FULL TABLES", -1);
            if (!tables.ok()) {
                warnings << QObject::tr("Tables/Views: %1").arg(original);
                tables = {};
            }
        }
        QVariantList objects;
        for (const QVariantList &row : tables.rows) {
            if (row.size() < 2) continue;
            const QString type = row[1].toString().toUpper();
            objects << QVariantMap{{"kind", type.contains("VIEW") ? "view" : "table"},
                                   {"name", row[0].toString()}};
        }

        QueryResult routines = db.query(
            "SELECT ROUTINE_NAME, ROUTINE_TYPE FROM information_schema.ROUTINES"
            " WHERE ROUTINE_SCHEMA=DATABASE() ORDER BY ROUTINE_NAME", -1);
        if (!routines.ok()) {
            warnings << QObject::tr("Functions/Procedures: %1").arg(routines.error);
        } else {
            for (const QVariantList &row : routines.rows) {
                objects << QVariantMap{{"kind", row[1].toString().toLower()}, {"name", row[0].toString()}};
            }
        }

        QueryResult triggers = db.query(
            "SELECT TRIGGER_NAME FROM information_schema.TRIGGERS"
            " WHERE TRIGGER_SCHEMA=DATABASE() ORDER BY TRIGGER_NAME", -1);
        if (!triggers.ok()) {
            warnings << QObject::tr("Triggers: %1").arg(triggers.error);
        } else {
            for (const QVariantList &row : triggers.rows)
                objects << QVariantMap{{"kind", "trigger"}, {"name", row[0].toString()}};
        }
        Q_UNUSED(dbName);
        return withKind("objects", QVariantMap{{"ok", true}, {"objects", objects},
                                               {"warnings", warnings}});
    });
}

void DbSession::loadTable(const QString &table, int offset, int limit, const QVariantList &filters,
                          const QString &orderBy)
{
    submit("table", [this, table, offset, limit, filters, orderBy](DbClient &db) {
        const QString dbName = m_conn.value("database").toString();
        QString pkError;
        const QStringList pk = loadPrimaryKey(db, dbName, table, &pkError);
        QString sql = "SELECT * FROM " + Sql::qualified(dbName, table)
                      + Sql::filterWhere(filters);
        if (!orderBy.isEmpty()) sql += " ORDER BY " + orderBy;
        sql += QString(" LIMIT %1 OFFSET %2").arg(limit + 1).arg(offset);
        QueryResult r = db.query(sql, limit + 1);
        if (!r.ok()) return withKind("table", errorMap(r.error, r.errorCode));
        QVariantMap m = resultMap(r);
        if (m["rows"].toList().size() > limit) {
            QVariantList rows = m["rows"].toList();
            while (rows.size() > limit) rows.removeLast();
            m["rows"] = rows;
            m["hasMore"] = true;
        } else {
            m["hasMore"] = false;
        }
        m["table"] = table;
        m["offset"] = offset;
        m["pk"] = pk;
        m["readOnly"] = pk.isEmpty();
        m["sql"] = sql;
        if (!pkError.isEmpty()) m["pkError"] = pkError;
        return withKind("table", m);
    });
}

void DbSession::loadDesign(const QString &kind, const QString &name)
{
    submit("design", [this, kind, name](DbClient &db) {
        const QString dbName = m_conn.value("database").toString();
        const QString full = Sql::qualified(dbName, name);
        QVariantMap m{{"ok", true}, {"kindName", kind}, {"name", name}};
        if (kind == "table" || kind == "view") {
            m["fields"] = resultMap(db.query("SHOW FULL COLUMNS FROM " + full, -1));
            m["indexes"] = resultMap(db.query("SHOW INDEX FROM " + full, -1));
            QueryResult fk = db.query(
                "SELECT CONSTRAINT_NAME, COLUMN_NAME, REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME"
                " FROM information_schema.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA=DATABASE()"
                " AND TABLE_NAME='" + db.escape(name) + "' AND REFERENCED_TABLE_NAME IS NOT NULL", -1);
            m["foreignKeys"] = fk.ok() ? resultMap(fk) : QVariantMap{{"ok", true}, {"rows", QVariantList()}};
            const QString cmd = kind == "view" ? "SHOW CREATE VIEW " : "SHOW CREATE TABLE ";
            m["ddl"] = resultMap(db.query(cmd + full, -1));
        } else if (kind == "function" || kind == "procedure") {
            QueryResult p = db.query(
                "SELECT PARAMETER_NAME, DTD_IDENTIFIER, PARAMETER_MODE"
                " FROM information_schema.PARAMETERS WHERE SPECIFIC_SCHEMA=DATABASE()"
                " AND SPECIFIC_NAME='" + db.escape(name) + "' ORDER BY ORDINAL_POSITION", -1);
            m["fields"] = p.ok() ? resultMap(p) : QVariantMap{{"ok", true}, {"rows", QVariantList()}};
            m["indexes"] = QVariantMap{{"ok", true}, {"rows", QVariantList()}};
            m["foreignKeys"] = QVariantMap{{"ok", true}, {"rows", QVariantList()}};
            m["ddl"] = resultMap(db.query(QString("SHOW CREATE %1 %2")
                                              .arg(kind.toUpper(), full), -1));
        } else {
            m["fields"] = QVariantMap{{"ok", true}, {"rows", QVariantList()}};
            m["indexes"] = QVariantMap{{"ok", true}, {"rows", QVariantList()}};
            m["foreignKeys"] = QVariantMap{{"ok", true}, {"rows", QVariantList()}};
            m["ddl"] = resultMap(db.query("SHOW CREATE TRIGGER " + full, -1));
        }
        return withKind("design", m);
    });
}

void DbSession::updateCell(const QString &table, const QString &column, const QString &value,
                           bool isNull, const QVariantMap &pk)
{
    submit("write", [this, table, column, value, isNull, pk](DbClient &db) {
        const QString dbName = m_conn.value("database").toString();
        const QString where = pkWhere(db, pk);
        if (where.isEmpty()) return withKind("write", errorMap(QObject::tr("This table has no primary key; editing is disabled.")));
        const QString lit = isNull ? QStringLiteral("NULL")
                                   : '\'' + db.escape(value) + '\'';
        const QString sql = "UPDATE " + Sql::qualified(dbName, table) + " SET "
                            + Sql::quoteId(column) + '=' + lit + " WHERE " + where + " LIMIT 1";
        QueryResult r = db.query(sql, -1);
        QVariantMap m = resultMap(r);
        m["sql"] = sql;
        m["row"] = 0;
        return withKind("write", m);
    });
}

void DbSession::insertRow(const QString &table, const QVariantMap &values)
{
    submit("write", [this, table, values](DbClient &db) {
        const QString dbName = m_conn.value("database").toString();
        QStringList cols, vals;
        for (auto it = values.begin(); it != values.end(); ++it) {
            cols << Sql::quoteId(it.key());
            vals << (it.value().isNull() || !it.value().isValid()
                         ? QStringLiteral("NULL") : '\'' + db.escape(it.value().toString()) + '\'');
        }
        QString sql = "INSERT INTO " + Sql::qualified(dbName, table);
        if (cols.isEmpty()) sql += " () VALUES ()";
        else sql += " (" + cols.join(',') + ") VALUES (" + vals.join(',') + ')';
        QueryResult r = db.query(sql, -1);
        QVariantMap m = resultMap(r);
        m["sql"] = sql;
        m["inserted"] = true;
        return withKind("write", m);
    });
}

void DbSession::deleteRow(const QString &table, const QVariantMap &pk)
{
    submit("write", [this, table, pk](DbClient &db) {
        const QString dbName = m_conn.value("database").toString();
        const QString where = pkWhere(db, pk);
        if (where.isEmpty()) return withKind("write", errorMap(QObject::tr("This table has no primary key; deleting is disabled.")));
        const QString sql = "DELETE FROM " + Sql::qualified(dbName, table)
                            + " WHERE " + where + " LIMIT 1";
        QueryResult r = db.query(sql, -1);
        QVariantMap m = resultMap(r);
        m["sql"] = sql;
        m["deleted"] = true;
        return withKind("write", m);
    });
}
