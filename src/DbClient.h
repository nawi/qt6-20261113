#pragma once
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

struct ResultColumn {
    QString name;
    QString type;
    bool binary = false;
};

struct QueryResult {
    QList<ResultColumn> columns;
    QList<QVariantList> rows;
    qint64 affected = 0;
    qint64 insertId = 0;
    bool truncated = false;
    qint64 elapsedMs = 0;
    int errorCode = 0;
    QString error;
    bool ok() const { return error.isEmpty(); }
};

// MariaDB Connector/C wrapper. One instance is used by one thread at a time.
class DbClient {
public:
    struct Config {
        QString host;
        quint16 port = 3306;
        QString user;
        QString password;
        QString database;
        bool ssl = false;      // On = encrypted, certificate is NOT verified
        QString httpUrl;       // non-empty = talk to the PHP tunnel agent instead of TCP
        QString httpToken;
        int connectTimeout = 10;
        int readTimeout = 300;
    };

    DbClient();
    ~DbClient();
    DbClient(const DbClient &) = delete;
    DbClient &operator=(const DbClient &) = delete;

    bool open(const Config &c, QString *error = nullptr);
    void close();
    bool isOpen() const { return m != nullptr || m_http; }
    QString serverVersion() const { return m_version; }
    QString sslCipher() const { return m_cipher; }
    unsigned threadId() const { return m_threadId; }
    const Config &config() const { return m_config; }

    // limit < 0 = no row cap, 0 = only count (no rows), >0 = cap rows (detects truncation).
    QueryResult query(const QString &sql, int limit = -1);
    QString escape(const QString &s) const;
    static QString quoteId(const QString &name);
    static QString friendlyError(int code, const QString &message);
    static bool isPrivilegeError(int code);

    // Test Connection: version + visible object counts. Never throws; check map["ok"].
    static QVariantMap test(const Config &c);

    // Object counts used by test(); counts are -1 when the server denies access.
    QVariantMap objectCounts();

private:
    bool connectWithCharset(const char *charset, QString *error);
    QString typeName(int mysqlType) const;
    void setSessionLimit(int rows);

    bool openHttp(QString *error);
    QueryResult queryHttp(const QString &sql, int limit);

    void *m = nullptr;   // MYSQL*
    bool m_http = false;
    Config m_config;
    QString m_version;
    QString m_cipher;
    unsigned m_threadId = 0;
    int m_sessionLimit = -1;
    int m_lastErrorCode = 0;
};
