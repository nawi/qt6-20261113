#include "DbClient.h"
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QObject>
#include <mysql.h>
#include <cstring>

namespace {
QString decodeCell(const char *data, unsigned long len, bool binary)
{
    Q_UNUSED(binary);
    const QByteArray bytes(data, int(len));
    // Metadata strings from SHOW/INFORMATION_SCHEMA often report the binary charset even
    // when they are plain UTF-8, so decode first and only fall back to base64.
    QStringDecoder dec(QStringDecoder::Utf8);
    QString s = dec.decode(bytes);
    if (!dec.hasError()) return s;
    return QStringLiteral("base64:") + QString::fromLatin1(bytes.toBase64());
}

}

DbClient::DbClient() = default;

DbClient::~DbClient() { close(); }

QString DbClient::quoteId(const QString &name)
{
    QString n = name;
    n.replace('`', "``");
    return '`' + n + '`';
}

QString DbClient::escape(const QString &s) const
{
    if (!m) {
        QString r = s;
        r.replace('\\', "\\\\").replace('\'', "\\'");
        return r;
    }
    const QByteArray in = s.toUtf8();
    QByteArray out(in.size() * 2 + 1, '\0');
    const unsigned long n = mysql_real_escape_string(static_cast<MYSQL *>(m), out.data(), in.constData(), in.size());
    out.resize(int(n));
    return QString::fromUtf8(out);
}

bool DbClient::isPrivilegeError(int code)
{
    return code == 1044 || code == 1142 || code == 1143 || code == 1227 || code == 1370;
}

QString DbClient::friendlyError(int code, const QString &message)
{
    if (isPrivilegeError(code))
        return QObject::tr("Not enough privileges: %1").arg(message);
    switch (code) {
    case 2002: case 2003: case 2005:
        return QObject::tr("Server unreachable: %1").arg(message);
    case 2006: case 2013:
        return QObject::tr("Connection lost: %1").arg(message);
    case 1045:
        return QObject::tr("Access denied: %1").arg(message);
    case 1049:
        return QObject::tr("Unknown database: %1").arg(message);
    default:
        return message;
    }
}

bool DbClient::connectWithCharset(const char *charset, QString *error)
{
    MYSQL *m = mysql_init(nullptr);
    if (!m) { if (error) *error = QStringLiteral("Out of memory"); return false; }

    unsigned connectTimeout = unsigned(m_config.connectTimeout > 0 ? m_config.connectTimeout : 10);
    unsigned readTimeout = unsigned(m_config.readTimeout > 0 ? m_config.readTimeout : 300);
    my_bool reconnect = 0;
    my_bool verify = 0;
    my_bool enforce = m_config.ssl ? 1 : 0;
    unsigned localInfile = 0;
    mysql_optionsv(m, MYSQL_OPT_CONNECT_TIMEOUT, &connectTimeout);
    mysql_optionsv(m, MYSQL_OPT_READ_TIMEOUT, &readTimeout);
    mysql_optionsv(m, MYSQL_OPT_WRITE_TIMEOUT, &readTimeout);
    mysql_optionsv(m, MYSQL_OPT_RECONNECT, &reconnect);
    mysql_optionsv(m, MYSQL_OPT_LOCAL_INFILE, &localInfile);
    mysql_optionsv(m, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);
    mysql_optionsv(m, MYSQL_OPT_SSL_ENFORCE, &enforce);
    mysql_optionsv(m, MYSQL_SET_CHARSET_NAME, charset);

    const QByteArray host = m_config.host.toUtf8();
    const QByteArray user = m_config.user.toUtf8();
    const QByteArray pass = m_config.password.toUtf8();
    const QByteArray db = m_config.database.toUtf8();

    if (!mysql_real_connect(m, host.constData(), user.constData(), pass.constData(), db.constData(),
                            m_config.port, nullptr, 0)) {
        m_lastErrorCode = mysql_errno(m);
        if (error) *error = friendlyError(m_lastErrorCode, QString::fromUtf8(mysql_error(m)));
        mysql_close(m);
        return false;
    }
    m = static_cast<MYSQL *>(m);
    this->m = m;
    m_version = QString::fromUtf8(mysql_get_server_info(m));
    m_threadId = mysql_thread_id(m);
    m_sessionLimit = -1;
    return true;
}

bool DbClient::open(const Config &c, QString *error)
{
    close();
    m_config = c;
    m_config.host = c.host.trimmed();
    m_config.user = c.user.trimmed();
    m_config.database = c.database.trimmed();

    QString firstError;
    if (!connectWithCharset("utf8mb4", &firstError)) {
        // Old servers may not know utf8mb4 (error 2019). Retry with utf8.
        if (m_lastErrorCode == 2019 || m_lastErrorCode == 1115) {
            if (!connectWithCharset("utf8", &firstError)) {
                if (error) *error = firstError;
                return false;
            }
        } else {
            if (error) *error = firstError;
            return false;
        }
    }

    // Reject silent TLS fallback: SSL On must actually be encrypted.
    if (m_config.ssl) {
        QueryResult r = query("SHOW STATUS LIKE 'Ssl_cipher'", -1);
        if (!r.ok() || r.rows.isEmpty() || r.rows.first().value(1).toString().isEmpty()) {
            close();
            if (error) *error = QObject::tr("TLS is not available on this server (disable SSL to connect in plaintext).");
            return false;
        }
        m_cipher = r.rows.first().value(1).toString();
    }
    return true;
}

void DbClient::close()
{
    if (m) {
        mysql_close(static_cast<MYSQL *>(m));
        m = nullptr;
    }
    m_version.clear();
    m_cipher.clear();
    m_threadId = 0;
    m_sessionLimit = -1;
}

QString DbClient::typeName(int t) const
{
    switch (t) {
    case MYSQL_TYPE_DECIMAL: case MYSQL_TYPE_NEWDECIMAL: return "decimal";
    case MYSQL_TYPE_TINY: return "tinyint";
    case MYSQL_TYPE_SHORT: return "smallint";
    case MYSQL_TYPE_LONG: return "int";
    case MYSQL_TYPE_FLOAT: return "float";
    case MYSQL_TYPE_DOUBLE: return "double";
    case MYSQL_TYPE_NULL: return "null";
    case MYSQL_TYPE_TIMESTAMP: return "timestamp";
    case MYSQL_TYPE_LONGLONG: return "bigint";
    case MYSQL_TYPE_INT24: return "mediumint";
    case MYSQL_TYPE_DATE: return "date";
    case MYSQL_TYPE_TIME: return "time";
    case MYSQL_TYPE_DATETIME: return "datetime";
    case MYSQL_TYPE_YEAR: return "year";
    case MYSQL_TYPE_NEWDATE: return "date";
    case MYSQL_TYPE_VARCHAR: return "varchar";
    case MYSQL_TYPE_BIT: return "bit";
    case MYSQL_TYPE_JSON: return "json";
    case MYSQL_TYPE_ENUM: return "enum";
    case MYSQL_TYPE_SET: return "set";
    case MYSQL_TYPE_TINY_BLOB: return "tinyblob";
    case MYSQL_TYPE_MEDIUM_BLOB: return "mediumblob";
    case MYSQL_TYPE_LONG_BLOB: return "longblob";
    case MYSQL_TYPE_BLOB: return "blob";
    case MYSQL_TYPE_VAR_STRING: return "varstring";
    case MYSQL_TYPE_STRING: return "char";
    case MYSQL_TYPE_GEOMETRY: return "geometry";
    default: return QStringLiteral("type%1").arg(t);
    }
}

void DbClient::setSessionLimit(int rows)
{
    // rows < 0 restores the server default (unlimited).
    if (rows == m_sessionLimit) return;
    const QString sql = rows < 0 ? QStringLiteral("SET SESSION sql_select_limit=DEFAULT")
                                 : QStringLiteral("SET SESSION sql_select_limit=%1").arg(rows);
    if (mysql_real_query(static_cast<MYSQL *>(m), sql.toUtf8().constData(), sql.toUtf8().size()) == 0) {
        MYSQL_RES *r = mysql_store_result(static_cast<MYSQL *>(m));
        if (r) mysql_free_result(r);
        m_sessionLimit = rows;
    }
}

QueryResult DbClient::query(const QString &sql, int limit)
{
    QueryResult out;
    QElapsedTimer timer;
    timer.start();
    if (!m) {
        out.error = QObject::tr("Not connected.");
        out.errorCode = -1;
        return out;
    }
    // sql_select_limit is N+1 so truncation can be detected.
    setSessionLimit(limit >= 0 ? limit + 1 : -1);

    const QByteArray q = sql.toUtf8();
    if (mysql_real_query(static_cast<MYSQL *>(m), q.constData(), q.size()) != 0) {
        out.errorCode = mysql_errno(static_cast<MYSQL *>(m));
        out.error = friendlyError(out.errorCode, QString::fromUtf8(mysql_error(static_cast<MYSQL *>(m))));
        out.elapsedMs = timer.elapsed();
        return out;
    }

    MYSQL_RES *res = mysql_store_result(static_cast<MYSQL *>(m));
    if (!res) {
        if (mysql_field_count(static_cast<MYSQL *>(m)) == 0) {
            out.affected = qint64(mysql_affected_rows(static_cast<MYSQL *>(m)));
            out.insertId = qint64(mysql_insert_id(static_cast<MYSQL *>(m)));
            out.elapsedMs = timer.elapsed();
            return out;
        }
        out.errorCode = mysql_errno(static_cast<MYSQL *>(m));
        if (out.errorCode == 0) out.errorCode = -1;
        out.error = friendlyError(out.errorCode, QString::fromUtf8(mysql_error(static_cast<MYSQL *>(m))));
        out.elapsedMs = timer.elapsed();
        return out;
    }

    const unsigned nfields = mysql_num_fields(res);
    MYSQL_FIELD *fields = mysql_fetch_fields(res);
    for (unsigned i = 0; i < nfields; ++i) {
        ResultColumn c;
        c.name = QString::fromUtf8(fields[i].name, int(fields[i].name_length));
        c.type = typeName(fields[i].type);
        // Numeric types also report charsetnr 63; only strings/blobs can actually be binary.
        const bool stringy = fields[i].type == MYSQL_TYPE_STRING || fields[i].type == MYSQL_TYPE_VAR_STRING
                             || fields[i].type == MYSQL_TYPE_VARCHAR || fields[i].type == MYSQL_TYPE_TINY_BLOB
                             || fields[i].type == MYSQL_TYPE_MEDIUM_BLOB || fields[i].type == MYSQL_TYPE_LONG_BLOB
                             || fields[i].type == MYSQL_TYPE_BLOB || fields[i].type == MYSQL_TYPE_BIT
                             || fields[i].type == MYSQL_TYPE_GEOMETRY || fields[i].type == MYSQL_TYPE_JSON;
        c.binary = fields[i].type == MYSQL_TYPE_TINY_BLOB || fields[i].type == MYSQL_TYPE_MEDIUM_BLOB
                   || fields[i].type == MYSQL_TYPE_LONG_BLOB || fields[i].type == MYSQL_TYPE_BLOB
                   || fields[i].type == MYSQL_TYPE_GEOMETRY || fields[i].type == MYSQL_TYPE_BIT
                   || (stringy && fields[i].charsetnr == 63);
        out.columns << c;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        unsigned long *lengths = mysql_fetch_lengths(res);
        QVariantList values;
        values.reserve(int(nfields));
        for (unsigned i = 0; i < nfields; ++i) {
            if (!row[i]) { values << QVariant(); continue; }
            values << decodeCell(row[i], lengths[i], out.columns[int(i)].binary);
        }
        out.rows << values;
    }
    if (limit >= 0 && out.rows.size() > limit) {
        out.truncated = true;
        while (out.rows.size() > limit) out.rows.removeLast();
    }
    mysql_free_result(res);
    out.elapsedMs = timer.elapsed();
    return out;
}

QVariantMap DbClient::objectCounts()
{
    QVariantMap counts{{"tables", -1}, {"views", -1}, {"functions", -1}, {"procedures", -1}, {"triggers", -1}};
    auto run = [&](const char *key, const QString &sql) {
        QueryResult r = query(sql, -1);
        if (r.ok() && !r.rows.isEmpty() && !r.rows.first().isEmpty())
            counts[key] = r.rows.first().first().toLongLong();
    };
    const QString db = quoteId(m_config.database);
    run("tables", "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_TYPE='BASE TABLE'");
    run("views", "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_TYPE='VIEW'");
    run("functions", "SELECT COUNT(*) FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA=DATABASE() AND ROUTINE_TYPE='FUNCTION'");
    run("procedures", "SELECT COUNT(*) FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA=DATABASE() AND ROUTINE_TYPE='PROCEDURE'");
    run("triggers", "SELECT COUNT(*) FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE()");
    Q_UNUSED(db);
    return counts;
}

QVariantMap DbClient::test(const Config &c)
{
    QVariantMap out;
    DbClient db;
    QString err;
    if (!db.open(c, &err)) {
        out["ok"] = false;
        out["error"] = err;
        return out;
    }
    out["ok"] = true;
    out["version"] = db.serverVersion();
    out["cipher"] = db.sslCipher();
    const QVariantMap counts = db.objectCounts();
    out.insert(counts);
    QStringList warnings;
    const QPair<const char *, QString> groups[] = {
        {"tables", "Tables"}, {"views", "Views"}, {"functions", "Functions"},
        {"procedures", "Procedures"}, {"triggers", "Triggers"}};
    for (const auto &g : groups)
        if (counts.value(g.first).toInt() < 0) warnings << g.second;
    out["warnings"] = warnings;
    return out;
}
