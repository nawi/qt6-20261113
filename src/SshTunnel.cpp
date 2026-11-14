#include "SshTunnel.h"
#include "ConnectionStore.h"
#include <QFutureWatcher>
#include <QSqlQuery>
#include <QtConcurrent>

namespace {
struct TunnelResult {
    quint64 handle = 0;
    quint16 port = 0;
    QString fingerprint;
    int code = 1;
    QString message;
};

QString savedFingerprint(int id)
{
    QSqlQuery q;
    q.prepare("SELECT ssh_fp FROM connections WHERE id=?");
    q.addBindValue(id);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}
}

TunnelManager::TunnelManager(QObject *parent) : QObject(parent) {}

TunnelManager::~TunnelManager()
{
    const auto ids = m_entries.keys();
    for (int id : ids) {
        if (m_entries[id].handle) ssh_bridge_disconnect(m_entries[id].handle);
    }
}

void TunnelManager::acquire(int id)
{
    Entry &e = m_entries[id];
    ++e.refs;
    if (e.port && e.handle) {
        emit ready(id, "127.0.0.1", e.port);
        return;
    }
    if (!e.starting) start(id);
}

void TunnelManager::start(int id)
{
    Entry &e = m_entries[id];
    e.starting = true;
    e.port = 0;

    auto *store = qobject_cast<ConnectionStore *>(parent());
    if (!store) {
        e.starting = false;
        emit failed(id, OtherError, tr("No connection store."), {});
        return;
    }
    const QVariantMap c = store->get(id);
    if (c.isEmpty()) {
        e.starting = false;
        emit failed(id, OtherError, tr("Connection not found."), {});
        return;
    }

    const QString expectedFp = savedFingerprint(id);
    auto *w = new QFutureWatcher<TunnelResult>(this);
    connect(w, &QFutureWatcher<TunnelResult>::finished, this, [this, w, id, expectedFp] {
        const TunnelResult r = w->result();
        w->deleteLater();
        Entry &en = m_entries[id];
        en.starting = false;
        en.fingerprint = r.fingerprint;
        if (r.code == Ok && r.handle) {
            en.handle = r.handle;
            en.port = r.port;
            if (en.refs <= 0) {   // released while starting
                stop(id);
                return;
            }
            emit ready(id, "127.0.0.1", int(r.port));
        } else {
            if (en.refs <= 0) { m_entries.remove(id); return; }
            emit failed(id, r.code, r.message, r.fingerprint);
        }
    });
    w->setFuture(QtConcurrent::run([c, expectedFp] {
        TunnelResult r;
        const QString expected = expectedFp;
        const QByteArray host = c.value("sshHost").toString().trimmed().toUtf8();
        const QByteArray user = c.value("sshUser").toString().trimmed().toUtf8();
        const QByteArray secret = c.value("sshSecret").toString().toUtf8();
        const QByteArray fp = expected.toUtf8();
        const QByteArray dbHost = c.value("host").toString().trimmed().toUtf8();
        QByteArray err(512, '\0');
        QByteArray outFp(256, '\0');
        unsigned short port = 0;
        int rc = 0;
        r.handle = ssh_bridge_connect(host.constData(), (unsigned short)c.value("sshPort").toUInt(),
                                      user.constData(), c.value("sshAuth").toInt(), secret.constData(),
                                      fp.constData(), dbHost.constData(),
                                      (unsigned short)c.value("port").toUInt(), 15,
                                      &port, outFp.data(), outFp.size(), &rc, err.data(), err.size());
        r.port = port;
        r.code = rc;
        r.fingerprint = QString::fromUtf8(outFp.constData());
        r.message = QString::fromUtf8(err.constData());
        return r;
    }));
}

void TunnelManager::release(int connectionId)
{
    auto it = m_entries.find(connectionId);
    if (it == m_entries.end()) return;
    if (--it->refs <= 0 && !it->starting) stop(connectionId);
}

void TunnelManager::stop(int id)
{
    auto it = m_entries.find(id);
    if (it == m_entries.end()) return;
    if (it->handle) ssh_bridge_disconnect(it->handle);
    m_entries.erase(it);
}

void TunnelManager::trustFingerprint(int connectionId, const QString &fingerprint)
{
    if (fingerprint.isEmpty()) return;
    QSqlQuery q;
    q.prepare("UPDATE connections SET ssh_fp=? WHERE id=?");
    q.addBindValue(fingerprint);
    q.addBindValue(connectionId);
    if (!q.exec()) return;
    auto it = m_entries.find(connectionId);
    if (it != m_entries.end() && !it->starting) start(connectionId);
}
