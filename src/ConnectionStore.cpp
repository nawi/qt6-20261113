#include "ConnectionStore.h"
#include "Probe.h"
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
    {"sshSecret", "ssh_secret_enc", true, "", false}, {"httpUrl", "http_url", false, "", false},
    {"httpToken", "http_token_enc", true, "", false},
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
    if (c.value("mode").toInt() != 0) {
        emit testFinished(false, tr("Testing tunnel modes is not available yet."));
        return;
    }
    m_testing = true;
    emit testingChanged();
    auto *w = new QFutureWatcher<QString>(this);
    connect(w, &QFutureWatcher<QString>::finished, this, [this, w] {
        const QString r = w->result();
        w->deleteLater();
        m_testing = false;
        emit testingChanged();
        emit testFinished(!r.startsWith("ERROR"), r.startsWith("ERROR") ? r.mid(7) : tr("Connected. Server version %1").arg(r));
    });
    w->setFuture(QtConcurrent::run([c] {
        return Probe::serverVersion(c.value("host").toString().trimmed(), c.value("port").toUInt(),
                                    c.value("user").toString().trimmed(), c.value("password").toString(),
                                    c.value("database").toString().trimmed(), c.value("ssl").toBool());
    }));
}
