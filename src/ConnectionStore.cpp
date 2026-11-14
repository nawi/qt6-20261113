#include "ConnectionStore.h"
#include "DbClient.h"
#include "SshTunnel.h"
#include "Vault.h"
#include <QFutureWatcher>
#include <QSqlQuery>
#include <QSqlError>
#include <QtConcurrent>
#include <QDateTime>

namespace {
struct F { const char *key, *col; bool secret; QString def; bool numeric; };
const F kFields[] = {
    {"name", "name", false, "", false}, {"mode", "mode", false, "0", true}, {"host", "host", false, "", false},
    {"port", "port", false, "3306", true}, {"user", "user", false, "", false},
    {"password", "password_enc", true, "", false}, {"database", "db", false, "", false},
    {"ssl", "ssl", false, "0", true}, {"color", "color", false, "#007AFF", false},
    {"sshHost", "ssh_host", false, "", false}, {"sshPort", "ssh_port", false, "22", true},
    {"sshUser", "ssh_user", false, "", false}, {"sshAuth", "ssh_auth", false, "0", true},
    {"sshSecret", "ssh_secret_enc", true, "", false}, {"sshFp", "ssh_fp", false, "", false},
    {"httpUrl", "http_url", false, "", false}, {"httpToken", "http_token_enc", true, "", false},
};
QString sel() { QStringList c{"id", "last_used"}; for (auto &f : kFields) c << f.col; return c.join(','); }
bool validPort(const QVariant &v) { int p = v.toInt(); return p >= 1 && p <= 65535; }
}

ConnectionStore::ConnectionStore(QObject *parent) : QAbstractListModel(parent)
{
    m_keys << "id" << "lastUsed";
    for (auto &f : kFields) if (!f.secret) m_keys << f.key;
    reload();
}

QHash<int, QByteArray> ConnectionStore::roleNames() const
{
    QHash<int, QByteArray> r;
    for (int i = 0; i < m_keys.size(); ++i) r[Qt::UserRole + i] = m_keys[i].toUtf8();
    return r;
}

QVariant ConnectionStore::data(const QModelIndex &i, int role) const
{
    const int k = role - Qt::UserRole;
    if (!i.isValid() || i.row() >= m_rows.size() || k < 0 || k >= m_keys.size()) return {};
    return m_rows[i.row()].value(m_keys[k]);
}

void ConnectionStore::reload()
{
    beginResetModel();
    m_rows.clear();
    QSqlQuery q("SELECT " + sel() + " FROM connections ORDER BY id");
    while (q.next()) {
        QVariantMap m{{"id", q.value(0)}, {"lastUsed", q.value(1)}};
        int c = 2;
        for (auto &f : kFields) { if (!f.secret) m[f.key] = q.value(c); ++c; }
        m_rows << m;
    }
    endResetModel();
    emit countChanged();
}

QString ConnectionStore::validate(const QVariantMap &c)
{
    auto s = [&](const char *k) { return c.value(k).toString().trimmed(); };
    if (s("name").isEmpty()) return tr("Name is required.");
    if (s("host").isEmpty()) return tr("Host is required.");
    if (!validPort(c.value("port"))) return tr("Port must be 1-65535.");
    if (s("user").isEmpty()) return tr("Username is required.");
    if (s("database").isEmpty()) return tr("Database is required.");
    const int mode = c.value("mode").toInt();
    if (mode < 0 || mode > 2) return tr("Invalid connection type.");
    if (mode == 1) {
        if (s("sshHost").isEmpty()) return tr("SSH host is required.");
        if (!validPort(c.value("sshPort"))) return tr("SSH port must be 1-65535.");
        if (s("sshUser").isEmpty()) return tr("SSH username is required.");
        const QString sec = c.value("sshSecret").toString();
        if (sec.isEmpty()) return c.value("sshAuth").toInt() == 1 ? tr("Private key is required.") : tr("SSH password is required.");
        if (c.value("sshAuth").toInt() == 1 && !sec.contains("-----BEGIN")) return tr("Private key must be PEM/OpenSSH text.");
    }
    if (mode == 2) {
        const QString u = s("httpUrl").toLower();
        if (!u.startsWith("http://") && !u.startsWith("https://")) return tr("Agent URL must start with http:// or https://.");
        if (c.value("httpToken").toString().isEmpty()) return tr("Agent token is required.");
    }
    return {};
}

QString ConnectionStore::save(const QVariantMap &c)
{
    if (const QString e = validate(c); !e.isEmpty()) return e;
    QStringList cols, ph;
    QVariantList vals;
    for (auto &f : kFields) {
        QVariant v = c.value(f.key);
        if (!v.isValid() || v.isNull()) v = f.numeric ? QVariant(f.def.toInt()) : QVariant(f.def);
        if (f.secret) {
            const QString enc = Vault::encrypt(v.toString());
            if (enc.isNull()) return tr("Secure storage unavailable; nothing was saved.");
            v = enc;
        } else if (v.typeId() == QMetaType::QString) {
            v = v.toString().trimmed();
            if (v.toString().isNull()) v = QStringLiteral("");   // null QString binds as SQL NULL; columns are NOT NULL
        } else if (v.typeId() == QMetaType::Bool) {
            v = v.toBool() ? 1 : 0;
        }
        cols << f.col; ph << "?"; vals << v;
    }
    const int id = c.value("id").toInt();
    QSqlQuery q;
    if (id > 0) {
        QStringList sets; for (auto &col : cols) sets << col + "=?";
        q.prepare("UPDATE connections SET " + sets.join(',') + " WHERE id=?");
        for (auto &v : vals) q.addBindValue(v);
        q.addBindValue(id);
    } else {
        q.prepare("INSERT INTO connections(" + cols.join(',') + ") VALUES(" + ph.join(',') + ")");
        for (auto &v : vals) q.addBindValue(v);
    }
    if (!q.exec()) return tr("Save failed: %1").arg(q.lastError().text());
    reload();
    return {};
}

void ConnectionStore::remove(int id)
{
    QSqlQuery q;
    q.prepare("DELETE FROM connections WHERE id=?");
    q.addBindValue(id);
    if (q.exec()) reload();
}

QVariantMap ConnectionStore::get(int id) const
{
    QSqlQuery q;
    q.prepare("SELECT " + sel() + " FROM connections WHERE id=?");
    q.addBindValue(id);
    QVariantMap m;
    if (!q.exec() || !q.next()) return m;
    m["id"] = q.value(0);
    int c = 2;
    for (auto &f : kFields) {
        m[f.key] = f.secret ? QVariant(Vault::decrypt(q.value(c).toString())) : q.value(c);
        ++c;
    }
    return m;
}

void ConnectionStore::testConnection(const QVariantMap &c)
{
    if (m_testing) return;
    if (const QString e = validate(c); !e.isEmpty()) { emit testFinished(false, e); return; }
    m_testing = true;
    emit testingChanged();
    auto *w = new QFutureWatcher<QVariantMap>(this);
    connect(w, &QFutureWatcher<QVariantMap>::finished, this, [this, w, c] {
        const QVariantMap r = w->result();
        w->deleteLater();
        m_testing = false;
        emit testingChanged();
        if (r.value("hostKeyCode").toInt() != 0) {
            emit hostKeyNeeded(r.value("fingerprint").toString(),
                               r.value("hostKeyCode").toInt() == 4);
            return;
        }
        if (!r.value("ok").toBool()) {
            emit testFinished(false, r.value("error").toString());
            return;
        }
        QStringList parts;
        const QPair<const char *, QString> groups[] = {
            {"tables", tr("Tables")}, {"views", tr("Views")}, {"functions", tr("Functions")},
            {"procedures", tr("Procedures")}, {"triggers", tr("Triggers")}};
        for (const auto &g : groups) {
            const int n = r.value(g.first).toInt();
            parts << tr("%1: %2").arg(g.second, n < 0 ? tr("n/a") : QString::number(n));
        }
        QString msg = tr("Connected. Server version %1\n%2").arg(r.value("version").toString(), parts.join(", "));
        if (!r.value("cipher").toString().isEmpty())
            msg += tr("\nTLS cipher: %1").arg(r.value("cipher").toString());
        if (c.value("mode").toInt() == 2 && c.value("httpUrl").toString().trimmed().startsWith("http://", Qt::CaseInsensitive))
            msg += tr("\nWarning: agent URL is not HTTPS; credentials travel unencrypted.");
        for (const QString &warn : r.value("warnings").toStringList())
            msg += tr("\nNot enough privileges to list %1").arg(warn);
        emit testFinished(true, msg);
    });
    w->setFuture(QtConcurrent::run([c] {
        DbClient::Config cfg;
        cfg.host = c.value("host").toString().trimmed();
        cfg.port = quint16(c.value("port").toUInt());
        cfg.user = c.value("user").toString().trimmed();
        cfg.password = c.value("password").toString();
        cfg.database = c.value("database").toString().trimmed();
        cfg.ssl = c.value("ssl").toBool();
        if (c.value("mode").toInt() == 2) {
            cfg.httpUrl = c.value("httpUrl").toString().trimmed();
            cfg.httpToken = c.value("httpToken").toString();
        }

        unsigned long long tunnel = 0;
        if (c.value("mode").toInt() == 1) {
            const QByteArray host = c.value("sshHost").toString().trimmed().toUtf8();
            const QByteArray user = c.value("sshUser").toString().trimmed().toUtf8();
            const QByteArray secret = c.value("sshSecret").toString().toUtf8();
            const QByteArray fp = c.value("sshFp").toString().toUtf8();
            const QByteArray dbHost = cfg.host.toUtf8();
            QByteArray err(512, '\0'), outFp(256, '\0');
            unsigned short port = 0;
            int rc = 0;
            tunnel = ssh_bridge_connect(host.constData(), (unsigned short)c.value("sshPort").toUInt(),
                                        user.constData(), c.value("sshAuth").toInt(), secret.constData(),
                                        fp.constData(), dbHost.constData(), cfg.port, 15,
                                        &port, outFp.data(), outFp.size(), &rc, err.data(), err.size());
            if (!tunnel) {
                QVariantMap fail;
                fail["ok"] = false;
                fail["error"] = QString::fromUtf8(err.constData());
                fail["fingerprint"] = QString::fromUtf8(outFp.constData());
                fail["hostKeyCode"] = (rc == 3 || rc == 4) ? rc : 0;
                return fail;
            }
            cfg.host = "127.0.0.1";
            cfg.port = port;
        }
        QVariantMap r = DbClient::test(cfg);
        if (tunnel) ssh_bridge_disconnect(tunnel);
        return r;
    }));
}
