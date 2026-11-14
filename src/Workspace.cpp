#include "Workspace.h"
#include "Db.h"
#include "ConnectionStore.h"
#include "DbSession.h"
#include "GridModel.h"
#include "SshTunnel.h"
#include "Sql.h"
#include <QSqlQuery>
#include <QDateTime>
#include <QSqlError>
#include <QTimer>
#include <QDebug>

namespace {
QString escapePreview(const QString &s)
{
    QString r = s;
    r.replace('\\', "\\\\").replace('\'', "\\'");
    return '\'' + r + '\'';
}
int nowSec() { return int(QDateTime::currentSecsSinceEpoch()); }
}

// ---- TableTab ---------------------------------------------------------------
TableTab::TableTab(Workspace *ws, const QString &table, const QString &objKind, const QVariantMap &conn,
                   const QString &host, int port, QObject *parent)
    : Tab("table", parent), conn(conn), m_ws(ws), m_session(new DbSession(this)),
      m_grid(new GridModel(this)), m_table(table), m_objKind(objKind), m_host(host), m_port(port)
{
    m_title = table;
    connectSession();
}

void TableTab::connectSession()
{
    connect(m_session, &DbSession::finished, this, [this](const QString &kind, const QVariantMap &r) {
        if (kind == "open") {
            if (!r.value("ok").toBool()) {
                m_error = r.value("error").toString();
                m_connected = false;
                if (m_ws) m_ws->notifyConnectionLost();
                emit changed();
                return;
            }
            m_connected = true;
            m_error.clear();
            if (m_ws) m_ws->notifyConnected(r.value("version").toString());
            emit changed();
            refresh();
        } else if (kind == "table") {
            m_loading = false;
            emit busyChanged();
            if (!r.value("ok").toBool()) {
                m_error = r.value("error").toString();
                const int code = r.value("errorCode").toInt();
                if (code == 2006 || code == 2013) {
                    m_connected = false;
                    if (m_ws) m_ws->notifyConnectionLost();
                }
                emit changed();
                return;
            }
            applyTable(r);
        } else if (kind == "write") {
            emit busyChanged();
            const bool ok = r.value("ok").toBool();
            if (!ok) {
                m_error = r.value("error").toString();
                const int code = r.value("errorCode").toInt();
                if (code == 2006 || code == 2013) {
                    m_connected = false;
                    if (m_ws) m_ws->notifyConnectionLost();
                }
                emit changed();
                emit writeFinished(false, m_error);
            } else {
                emit writeFinished(true, tr("Done."));
                load();
            }
        }
    });
    connect(m_session, &DbSession::busyChanged, this, &TableTab::busyChanged);
    m_session->open(conn, m_host, m_port);
}

bool TableTab::busy() const { return m_loading || (m_session && m_session->busy()); }
QObject *TableTab::gridObject() const { return m_grid; }

void TableTab::shutdown() { if (m_session) m_session->close(); }

void TableTab::load()
{
    if (!m_connected) return;
    if (m_loading) return;
    m_loading = true;
    m_error.clear();
    emit busyChanged();
    emit changed();
    QStringList searchCols;
    for (const QVariant &c : m_columns) {
        const QVariantMap cm = c.toMap();
        if (!cm.value("binary").toBool()) searchCols << cm.value("name").toString();
    }
    m_session->loadTable(m_table, m_offset, qMax(10, Db::get("pageSize", "100").toInt()), m_filters,
                         m_sortName.isEmpty() ? QString() : Sql::quoteId(m_sortName) + (m_sortDesc ? " DESC" : " ASC"),
                         m_search, searchCols);
}

void TableTab::refresh()
{
    m_offset = 0;
    if (!m_connected) {
        m_loading = true;
        m_error.clear();
        emit busyChanged();
        emit changed();
        m_session->open(conn, m_host, m_port);
        return;
    }
    load();
}

void TableTab::nextPage()
{
    if (!m_hasMore) return;
    m_offset += qMax(10, Db::get("pageSize", "100").toInt());
    load();
}

void TableTab::prevPage()
{
    if (m_offset <= 0) return;
    m_offset = qMax(0, m_offset - qMax(10, Db::get("pageSize", "100").toInt()));
    load();
}

void TableTab::setFilters(const QVariantList &filters)
{
    m_filters = filters;
    m_offset = 0;
    load();
}

void TableTab::clearFilters()
{
    m_filters.clear();
    m_offset = 0;
    load();
}

void TableTab::toggleSort(int column)
{
    if (column < 0 || column >= m_columns.size()) return;
    const QString name = m_columns[column].toMap().value("name").toString();
    if (m_sortName == name) {
        if (!m_sortDesc) m_sortDesc = true;
        else { m_sortName.clear(); m_sortCol = -1; m_sortDesc = false; }
    } else {
        m_sortName = name;
        m_sortDesc = false;
    }
    if (!m_sortName.isEmpty()) m_sortCol = column;
    m_offset = 0;
    load();
}

void TableTab::setSearch(const QString &text)
{
    if (text == m_search) return;
    m_search = text;
    m_offset = 0;
    load();
}

void TableTab::applyTable(const QVariantMap &r)
{
    m_grid->setResult(r);
    m_pk = r.value("pk").toStringList();
    m_readOnly = r.value("readOnly").toBool();
    m_hasMore = r.value("hasMore").toBool();
    m_offset = r.value("offset").toInt();
    m_columns.clear();
    for (const QVariant &c : r.value("columns").toList()) m_columns << c;
    if (!r.value("pkError").toString().isEmpty())
        m_error = tr("Primary key unknown (%1); grid is read-only.").arg(r.value("pkError").toString());
    else m_error.clear();
    emit changed();
}

void TableTab::openDesign() { m_ws->openDesign(m_objKind, m_table); }

QVariantMap TableTab::pkForRow(int row) const
{
    QVariantMap pk;
    for (const QString &col : m_pk) {
        const int idx = m_grid->headers().indexOf(col);
        if (idx < 0) continue;
        pk[col] = m_grid->cell(row, idx);
    }
    return pk;
}

void TableTab::updateCell(int row, const QString &column, const QString &value, bool isNull)
{
    m_session->updateCell(m_table, column, value, isNull, pkForRow(row));
    emit busyChanged();
}

void TableTab::insertRow(const QVariantMap &values)
{
    m_session->insertRow(m_table, values);
    emit busyChanged();
}

QString TableTab::deletePreview(int row) const
{
    const QVariantMap pk = pkForRow(row);
    QStringList parts;
    for (auto it = pk.begin(); it != pk.end(); ++it)
        parts << Sql::quoteId(it.key()) + (it.value().isNull() ? " IS NULL"
                                                                : "=" + escapePreview(it.value().toString()));
    return "DELETE FROM " + Sql::qualified(conn.value("database").toString(), m_table)
           + " WHERE " + parts.join(" AND ") + " LIMIT 1";
}

void TableTab::deleteRow(int row)
{
    m_session->deleteRow(m_table, pkForRow(row));
    emit busyChanged();
}

// ---- QueryTab ---------------------------------------------------------------
QueryTab::QueryTab(Workspace *ws, const QVariantMap &conn, const QString &host, int port,
                   const QString &extra, QObject *parent)
    : Tab("query", parent), m_ws(ws), m_session(new DbSession(this)), m_grid(new GridModel(this)),
      m_conn(conn), m_host(host), m_port(port), m_editor(extra)
{
    m_title = tr("SQL");
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(600);
    connect(m_saveTimer, &QTimer::timeout, this, [this] { if (m_ws) m_ws->saveTabs(); });
    connectSession();
    loadHistory();
}

void QueryTab::connectSession()
{
    connect(m_session, &DbSession::finished, this, [this](const QString &kind, const QVariantMap &r) {
        if (kind == "open") {
            if (!r.value("ok").toBool()) {
                m_message = r.value("error").toString();
                emit changed();
                return;
            }
            m_connected = true;
        } else if (kind == "query") {
            emit busyChanged();
            if (!r.value("ok").toBool()) {
                m_message = r.value("error").toString();
                m_grid->clear();
                emit changed();
                return;
            }
            m_grid->setResult(r);
            const int n = m_grid->rowCount();
            m_duration = r.value("elapsedMs").toLongLong();
            m_message = tr("%1 row(s) in %2 ms").arg(n).arg(m_duration);
            if (r.value("truncated").toBool()) m_message += tr(" (row limit reached)");
            if (r.value("affected").toLongLong() > 0)
                m_message += tr("; %1 affected").arg(r.value("affected").toLongLong());
            emit changed();
            QSqlQuery h;
            h.prepare("INSERT INTO query_history(conn_id, sql, ts) VALUES(?,?,?)");
            h.addBindValue(m_conn.value("id").toInt());
            h.addBindValue(m_editor);
            h.addBindValue(nowSec());
            if (h.exec()) loadHistory();
        }
    });
    connect(m_session, &DbSession::busyChanged, this, &QueryTab::busyChanged);
    m_session->open(m_conn, m_host, m_port);
}

bool QueryTab::busy() const { return m_session && m_session->busy(); }
QObject *QueryTab::gridObject() const { return m_grid; }
QString QueryTab::saveExtra() const { return m_editor; }
void QueryTab::shutdown() { if (m_session) m_session->close(); }

void QueryTab::setEditorText(const QString &text)
{
    if (m_editor == text) return;
    m_editor = text;
    if (m_saveTimer) m_saveTimer->start();
    emit textChanged();
}

void QueryTab::run()
{
    const QString g = guardMessage();
    if (!g.isEmpty()) { m_message = g; emit changed(); return; }
    m_session->runQuery(m_editor, qMax(1, Db::get("rowLimit", "100").toInt()));
    emit busyChanged();
}

void QueryTab::cancel()
{
    if (m_session) m_session->cancel();
}

void QueryTab::clearResult()
{
    m_grid->clear();
    m_message.clear();
    emit changed();
}

bool QueryTab::needsConfirm() const
{
    const QString u = m_editor.trimmed().toUpper();
    return u.startsWith("DELETE") || u.startsWith("UPDATE");
}

QString QueryTab::guardMessage() const { return Sql::guardWrite(m_editor); }

void QueryTab::loadHistory()
{
    QSqlQuery q;
    q.prepare("SELECT id, sql, ts FROM query_history WHERE conn_id=? ORDER BY id DESC LIMIT 50");
    q.addBindValue(m_conn.value("id").toInt());
    m_history.clear();
    if (q.exec()) {
        while (q.next())
            m_history << QVariantMap{{"id", q.value(0)}, {"sql", q.value(1)}, {"ts", q.value(2)}};
    }
    emit historyChanged();
}

void QueryTab::useHistory(int index)
{
    if (index < 0 || index >= m_history.size()) return;
    m_editor = m_history[index].toMap().value("sql").toString();
    emit textChanged();
}

void QueryTab::deleteHistory(int index)
{
    if (index < 0 || index >= m_history.size()) return;
    QSqlQuery q;
    q.prepare("DELETE FROM query_history WHERE id=?");
    q.addBindValue(m_history[index].toMap().value("id"));
    if (q.exec()) loadHistory();
}

// ---- DesignTab --------------------------------------------------------------
DesignTab::DesignTab(Workspace *ws, const QString &kind, const QString &name, const QVariantMap &conn,
                     const QString &host, int port, QObject *parent)
    : Tab("design", parent), m_ws(ws), m_session(new DbSession(this)), m_fields(new GridModel(this)),
      m_indexes(new GridModel(this)), m_fks(new GridModel(this)), m_objKind(kind), m_name(name)
{
    m_title = name;
    connect(m_session, &DbSession::finished, this, [this](const QString &k, const QVariantMap &r) {
        if (k == "open") {
            if (!r.value("ok").toBool()) { m_error = r.value("error").toString(); emit changed(); return; }
            m_connected = true;
            m_session->loadDesign(m_objKind, m_name);
        } else if (k == "design") {
            emit busyChanged();
            if (!r.value("ok").toBool()) { m_error = r.value("error").toString(); emit changed(); return; }
            m_fields->setResult(r.value("fields").toMap());
            m_indexes->setResult(r.value("indexes").toMap());
            m_fks->setResult(r.value("foreignKeys").toMap());
            const QVariantMap ddl = r.value("ddl").toMap();
            m_ddl.clear();
            if (ddl.value("ok").toBool()) {
                const QVariantList rows = ddl.value("rows").toList();
                if (!rows.isEmpty()) {
                    const QVariantList row = rows.first().toList();
                    for (const QVariant &v : row)
                        if (!v.isNull() && v.toString().size() > m_ddl.size()) m_ddl = v.toString();
                }
            } else {
                m_ddl = ddl.value("error").toString();
            }
            emit changed();
        }
    });
    connect(m_session, &DbSession::busyChanged, this, &DesignTab::busyChanged);
    m_session->open(conn, host, port);
}

bool DesignTab::busy() const { return m_session && m_session->busy(); }
QObject *DesignTab::fieldsObject() const { return m_fields; }
QObject *DesignTab::indexesObject() const { return m_indexes; }
QObject *DesignTab::fksObject() const { return m_fks; }
void DesignTab::shutdown() { if (m_session) m_session->close(); }
void DesignTab::reload() { if (m_connected) m_session->loadDesign(m_objKind, m_name); }

// ---- TabModel ---------------------------------------------------------------
QVariant TabModel::data(const QModelIndex &i, int role) const
{
    if (!i.isValid() || i.row() >= m_tabs.size()) return {};
    Tab *t = m_tabs[i.row()];
    switch (role) {
    case TitleRole: return t->title();
    case KindRole: return t->kind();
    case BusyRole: return t->busy();
    case TabRole: return QVariant::fromValue<QObject *>(t);
    default: return {};
    }
}

QHash<int, QByteArray> TabModel::roleNames() const
{
    return {{TitleRole, "title"}, {KindRole, "kind"}, {BusyRole, "busy"}, {TabRole, "tab"}};
}

QObject *TabModel::tabAt(int index) const { return at(index); }

void TabModel::add(Tab *tab)
{
    beginInsertRows({}, m_tabs.size(), m_tabs.size());
    m_tabs << tab;
    endInsertRows();
    connect(tab, &Tab::titleChanged, this, [this] {
        if (!m_tabs.isEmpty()) emit dataChanged(index(0), index(m_tabs.size() - 1), {TitleRole});
    });
    connect(tab, &Tab::busyChanged, this, [this] {
        if (!m_tabs.isEmpty()) emit dataChanged(index(0), index(m_tabs.size() - 1), {BusyRole});
    });
    emit countChanged();
}

void TabModel::removeAt(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    beginRemoveRows({}, index, index);
    Tab *t = m_tabs.takeAt(index);
    endRemoveRows();
    t->shutdown();
    t->deleteLater();
    emit countChanged();
}

void TabModel::clear()
{
    beginResetModel();
    const auto tabs = m_tabs;
    m_tabs.clear();
    endResetModel();
    for (Tab *t : tabs) { t->shutdown(); t->deleteLater(); }
    emit countChanged();
}

// ---- ObjectModel ------------------------------------------------------------
QVariant ObjectModel::data(const QModelIndex &i, int role) const
{
    if (!i.isValid() || i.row() >= m_rows.size()) return {};
    const QVariantMap row = m_rows[i.row()].toMap();
    switch (role) {
    case NameRole: return row.value("name");
    case KindRole: return row.value("kind");
    case SectionRole: return row.value("section");
    default: return {};
    }
}

QHash<int, QByteArray> ObjectModel::roleNames() const
{
    return {{NameRole, "name"}, {KindRole, "kind"}, {SectionRole, "section"}};
}

QString ObjectModel::nameAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows[row].toMap().value("name").toString() : QString();
}

QString ObjectModel::kindAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows[row].toMap().value("kind").toString() : QString();
}

void ObjectModel::setObjects(const QVariantList &objects, const QStringList &warnings)
{
    m_all = objects;
    m_warnings = warnings;
    setFilter(m_filter);
}

void ObjectModel::setFilter(const QString &text)
{
    m_filter = text.trimmed();
    beginResetModel();
    m_rows.clear();
    const QStringList order{"table", "view", "function", "procedure", "trigger"};
    const QStringList sections{tr("Tables"), tr("Views"), tr("Functions"), tr("Procedures"), tr("Triggers")};
    for (int k = 0; k < order.size(); ++k) {
        for (const QVariant &o : m_all) {
            const QVariantMap map = o.toMap();
            if (map.value("kind").toString() != order[k]) continue;
            if (!m_filter.isEmpty() && !map.value("name").toString().contains(m_filter, Qt::CaseInsensitive))
                continue;
            m_rows << QVariantMap{{"name", map.value("name")}, {"kind", map.value("kind")},
                                  {"section", sections[k]}};
        }
    }
    endResetModel();
    emit changed();
}

// ---- Workspace --------------------------------------------------------------
Workspace::Workspace(QObject *parent)
    : QObject(parent), m_objects(new ObjectModel(this)), m_tabs(new TabModel(this)),
      m_session(new DbSession(this))
{
    connect(m_session, &DbSession::finished, this, [this](const QString &kind, const QVariantMap &r) {
        if (kind != "open" && kind != "objects") return;
        if (kind == "open") {
            m_opening = false;
            if (!r.value("ok").toBool()) {
                setError(r.value("error").toString());
                return;
            }
            m_opened = true;
            m_status = tr("Connected · %1").arg(r.value("version").toString());
            QSqlQuery u;
            u.prepare("UPDATE connections SET last_used=? WHERE id=?");
            u.addBindValue(nowSec());
            u.addBindValue(m_connId);
            u.exec();
            if (!m_restored) {
                m_restored = true;
                if (m_tabs->rowCount() == 0) restoreTabs();
            }
            m_session->loadObjects();
            emit changed();
        } else {
            if (!r.value("ok").toBool()) {
                setError(r.value("error").toString());
                return;
            }
            m_objects->setObjects(r.value("objects").toList(),
                                  r.value("warnings").toStringList());
            m_warnings = r.value("warnings").toStringList();
            emit changed();
        }
    });
    connect(m_session, &DbSession::busyChanged, this, [this] {
        if (m_opening && m_opened) { m_opening = false; emit changed(); }
    });
}

Workspace::~Workspace()
{
    m_tabs->clear();
    if (m_tunnelHeld && m_tunnels) {
        QMetaObject::invokeMethod(m_tunnels, "release", Q_ARG(int, m_connId));
    }
}

void Workspace::setConnId(int id) { if (m_connId != id) { m_connId = id; emit connIdChanged(); } }

void Workspace::setStore(QObject *store)
{
    if (m_store == store) return;
    m_store = store;
    emit storeChanged();
}

void Workspace::setTunnels(QObject *t)
{
    if (m_tunnels == t) return;
    m_tunnels = t;
    auto *tm = qobject_cast<TunnelManager *>(t);
    if (tm) {
        connect(tm, &TunnelManager::ready, this, [this](int id, const QString &host, int port) {
            if (id != m_connId) return;
            m_tunnelHeld = true;
            m_status = tr("SSH tunnel on %1:%2").arg(host).arg(port);
            connectDatabase(host, port);
        });
        connect(tm, &TunnelManager::failed, this, [this](int id, int code, const QString &msg, const QString &fp) {
            if (id != m_connId) return;
            if (code == TunnelManager::HostKeyUnknown || code == TunnelManager::HostKeyMismatch) {
                m_opening = false;
                emit tunnelPrompt(code, msg, fp);
                return;
            }
            setError(msg);
        });
    }
    emit storeChanged();
}

void Workspace::start()
{
    if (m_connId <= 0 || !m_store) { setError(tr("Connection not found.")); return; }
    auto *store = qobject_cast<ConnectionStore *>(m_store);
    if (!store) { setError(tr("Connection store unavailable.")); return; }
    m_conn = store->get(m_connId);
    if (m_conn.isEmpty()) { setError(tr("Connection not found.")); return; }
    m_name = m_conn.value("name").toString();
    const int mode = m_conn.value("mode").toInt();
    m_detail = QString("%1:%2 · %3 · %4")
                   .arg(m_conn.value("host").toString())
                   .arg(m_conn.value("port").toInt())
                   .arg(m_conn.value("database").toString())
                   .arg(mode == 1 ? tr("SSH") : mode == 2 ? tr("HTTP") : tr("Direct"));
    m_opening = true;
    m_opened = false;
    m_error.clear();
    emit changed();
    beginTunnelOrConnect();
}

void Workspace::retry() { start(); }

void Workspace::stop()
{
    m_session->close();
    m_tabs->clear();
    if (m_tunnelHeld && m_tunnels) {
        QMetaObject::invokeMethod(m_tunnels, "release", Q_ARG(int, m_connId));
        m_tunnelHeld = false;
    }
}

void Workspace::beginTunnelOrConnect()
{
    const int mode = m_conn.value("mode").toInt();
    if (mode == 1) {
        if (!m_tunnels) { m_opening = false; setError(tr("SSH tunnel unavailable.")); return; }
        m_status = tr("Opening SSH tunnel…");
        emit changed();
        QMetaObject::invokeMethod(m_tunnels, "acquire", Q_ARG(int, m_connId));
        return;
    }
    connectDatabase(m_conn.value("host").toString(), m_conn.value("port").toInt());
}

void Workspace::connectDatabase(const QString &host, int port)
{
    m_connectHost = host;
    m_connectHostPort = port;
    m_status = tr("Connecting…");
    emit changed();
    m_session->open(m_conn, host, port);
}

void Workspace::trustTunnel(const QString &fingerprint)
{
    if (!m_tunnels || fingerprint.isEmpty()) return;
    m_pendingFp = fingerprint;
    QMetaObject::invokeMethod(m_tunnels, "trustFingerprint",
                              Q_ARG(int, m_connId), Q_ARG(QString, fingerprint));
    m_status = tr("Retrying SSH…");
    emit changed();
}

void Workspace::setCurrentIndex(int i)
{
    if (m_current == i) return;
    m_current = i;
    persistTabs();
    emit currentChanged();
}

void Workspace::openTable(const QString &table, const QString &objKind)
{
    addTab(new TableTab(this, table, objKind, m_conn, m_connectHost, m_connectHostPort, this), true);
}

void Workspace::openDesign(const QString &kind, const QString &name)
{
    addTab(new DesignTab(this, kind, name, m_conn, m_connectHost, m_connectHostPort, this), true);
}

void Workspace::openQuery()
{
    addTab(new QueryTab(this, m_conn, m_connectHost, m_connectHostPort, {}, this), true);
}

void Workspace::addTab(Tab *tab, bool persist)
{
    m_current = m_tabs->rowCount();
    m_tabs->add(tab);
    emit currentChanged();
    if (persist) persistTabs();
}

void Workspace::closeTab(int index)
{
    m_tabs->removeAt(index);
    if (m_current >= m_tabs->rowCount()) m_current = m_tabs->rowCount() - 1;
    emit currentChanged();
    persistTabs();
}

void Workspace::setFilter(const QString &text) { m_objects->setFilter(text); }

void Workspace::saveTabs() { persistTabs(); }

QObject *Workspace::objectsObject() const { return m_objects; }
QObject *Workspace::tabModelObject() const { return m_tabs; }

void Workspace::restoreTabs()
{
    QSqlQuery q;
    q.prepare("SELECT kind, ref, extra, title FROM tabs WHERE conn_id=? ORDER BY pos, id");
    q.addBindValue(m_connId);
    if (!q.exec()) return;
    QList<QVariantMap> rows;
    while (q.next())
        rows << QVariantMap{{"kind", q.value(0)}, {"ref", q.value(1)}, {"extra", q.value(2)}};
    for (const QVariantMap &r : rows) {
        const QString kind = r.value("kind").toString();
        const QString ref = r.value("ref").toString();
        if (kind == "table") addTab(new TableTab(this, ref, "table", m_conn, m_connectHost,
                                                 m_connectHostPort, this), false);
        else if (kind == "query") addTab(new QueryTab(this, m_conn, m_connectHost, m_connectHostPort,
                                                       r.value("extra").toString(), this), false);
        else if (kind == "design") {
            const QStringList parts = ref.split('|');
            const QString ok = parts.size() > 1 ? parts[0] : QStringLiteral("table");
            const QString name = parts.size() > 1 ? parts[1] : ref;
            addTab(new DesignTab(this, ok, name, m_conn, m_connectHost, m_connectHostPort, this), false);
        }
    }
    if (m_tabs->rowCount() > 0) {
        m_current = 0;
        emit currentChanged();
    }
}

void Workspace::persistTabs()
{
    QSqlQuery del;
    del.prepare("DELETE FROM tabs WHERE conn_id=?");
    del.addBindValue(m_connId);
    if (!del.exec()) { qWarning() << "tabs delete failed" << del.lastError().text(); return; }
    for (int i = 0; i < m_tabs->rowCount(); ++i) {
        Tab *t = m_tabs->at(i);
        if (!t) continue;
        QSqlQuery ins;
        ins.prepare("INSERT INTO tabs(conn_id, kind, ref, extra, title, pos) VALUES(?,?,?,?,?,?)");
        auto str = [](const QString &v) { return v.isNull() ? QStringLiteral("") : v; };
        ins.addBindValue(m_connId);
        ins.addBindValue(str(t->kind()));
        ins.addBindValue(str(t->saveRef()));
        ins.addBindValue(str(t->saveExtra()));
        ins.addBindValue(str(t->title()));
        ins.addBindValue(i);
        ins.exec();
    }
}

void Workspace::setError(const QString &e)
{
    m_error = e;
    m_opening = false;
    m_status = tr("Failed");
    emit changed();
}

void Workspace::notifyConnectionLost()
{
    m_status = tr("Connection lost");
    m_error = tr("Connection lost");
    emit changed();
}

void Workspace::notifyConnected(const QString &version)
{
    m_error.clear();
    m_status = tr("Connected · %1").arg(version);
    emit changed();
}
