#pragma once
#include "DbClient.h"
#include <QAtomicInt>
#include <QFutureWatcher>
#include <QObject>
#include <QQueue>
#include <QVariantMap>
#include <functional>

class DbClient;

// Async facade over one DbClient. Jobs run one at a time on a worker thread; results
// are delivered on the GUI thread with a `kind` tag.
class DbSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString version READ version NOTIFY connectedChanged)
public:
    explicit DbSession(QObject *parent = nullptr);
    ~DbSession() override;

    bool busy() const { return m_busy || !m_queue.isEmpty(); }
    bool connected() const { return m_connected; }
    QString version() const { return m_version; }
    QString lastError() const { return m_lastError; }

    // `conn` is a decrypted ConnectionStore map; host/port may point at an SSH tunnel.
    void open(const QVariantMap &conn, const QString &host, int port);
    void close();
    void cancel();

    void runQuery(const QString &sql, int limit);
    void loadObjects();
    void loadTable(const QString &table, int offset, int limit, const QVariantList &filters,
                   const QString &orderBy = {});
    void loadDesign(const QString &kind, const QString &name);
    void updateCell(const QString &table, const QString &column, const QString &value, bool isNull,
                    const QVariantMap &pk);
    void insertRow(const QString &table, const QVariantMap &values);
    void deleteRow(const QString &table, const QVariantMap &pk);

    QString database() const { return m_conn.value("database").toString(); }
    QString connectionLabel() const;

signals:
    void busyChanged();
    void connectedChanged();
    void finished(const QString &kind, const QVariantMap &result);

private:
    void submit(const QString &kind, std::function<QVariantMap(DbClient &)> job);
    void pump();
    void buildConfig(const QVariantMap &conn, const QString &host, int port);
    void deliver(const QString &kind, const QVariantMap &result);

    QVariantMap m_conn;
    QString m_host;
    int m_port = 3306;
    QString m_version;
    QString m_lastError;
    bool m_connected = false;
    bool m_busy = false;
    QQueue<QPair<QString, std::function<QVariantMap(DbClient &)>>> m_queue;
    QFutureWatcher<QVariantMap> *m_watcher = nullptr;
    DbClient m_client;
    DbClient::Config m_cfg;
    QAtomicInt m_tid;
};
