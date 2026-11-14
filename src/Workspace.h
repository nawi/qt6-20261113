#pragma once
#include <QAbstractListModel>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class QTimer;

class DbSession;
class GridModel;
class Workspace;

// ---- tabs -------------------------------------------------------------------
class Tab : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString kind READ kind CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    Tab(const QString &kind, QObject *parent = nullptr) : QObject(parent), m_kind(kind) {}
    QString title() const { return m_title; }
    QString kind() const { return m_kind; }
    virtual bool busy() const { return false; }
    virtual QString saveExtra() const { return {}; }
    virtual QString saveRef() const { return m_title; }
    virtual void shutdown() {}
signals:
    void titleChanged();
    void busyChanged();

protected:
    QString m_title;
    QString m_kind;
};

class TableTab : public Tab {
    Q_OBJECT
    Q_PROPERTY(QObject *grid READ gridObject CONSTANT)
    Q_PROPERTY(int offset READ offset NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString table READ table CONSTANT)
    Q_PROPERTY(QVariantList columns READ columns NOTIFY changed)
    Q_PROPERTY(QVariantList filters READ filters NOTIFY changed)
    Q_PROPERTY(int pkCount READ pkCount NOTIFY changed)
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY changed)
    Q_PROPERTY(bool sortDesc READ sortDesc NOTIFY changed)
    Q_PROPERTY(QString search READ search NOTIFY changed)
public:
    TableTab(Workspace *ws, const QString &table, const QString &objKind, const QVariantMap &conn,
             const QString &host, int port, QObject *parent = nullptr);
    bool busy() const override;
    QObject *gridObject() const;
    GridModel *grid() const { return m_grid; }
    QString table() const { return m_table; }
    int offset() const { return m_offset; }
    bool hasMore() const { return m_hasMore; }
    bool readOnly() const { return m_readOnly; }
    QString error() const { return m_error; }
    QVariantList columns() const { return m_columns; }
    QVariantList filters() const { return m_filters; }
    int pkCount() const { return m_pk.size(); }
    int sortColumn() const { return m_columns.isEmpty() ? -1 : m_sortCol; }
    bool sortDesc() const { return m_sortDesc; }
    QString search() const { return m_search; }
    void shutdown() override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void prevPage();
    Q_INVOKABLE void setFilters(const QVariantList &filters);
    Q_INVOKABLE void clearFilters();
    Q_INVOKABLE void toggleSort(int column);   // none -> ASC -> DESC -> none
    Q_INVOKABLE void setSearch(const QString &text);
    Q_INVOKABLE void openDesign();
    Q_INVOKABLE void updateCell(int row, const QString &column, const QString &value, bool isNull);
    Q_INVOKABLE void insertRow(const QVariantMap &values);
    Q_INVOKABLE void deleteRow(int row);
    Q_INVOKABLE QString deletePreview(int row) const;
    Q_INVOKABLE QVariantMap pkForRow(int row) const;

signals:
    void changed();
    void writeFinished(bool ok, const QString &message);

private:
    void load();
    void applyTable(const QVariantMap &r);
    void connectSession();
    QVariantMap conn;
    Workspace *m_ws;
    DbSession *m_session;
    GridModel *m_grid;
    QString m_table;
    QString m_objKind;
    QString m_host;
    int m_port;
    QStringList m_pk;
    QVariantList m_columns;
    QVariantList m_filters;
    int m_offset = 0;
    int m_sortCol = -1;
    bool m_sortDesc = false;
    QString m_search;
    QString m_sortName;
    bool m_hasMore = false;
    bool m_readOnly = true;
    bool m_connected = false;
    bool m_loading = false;
    QString m_error;
};

class QueryTab : public Tab {
    Q_OBJECT
    Q_PROPERTY(QString editorText READ editorText WRITE setEditorText NOTIFY textChanged)
    Q_PROPERTY(QObject *grid READ gridObject CONSTANT)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(qint64 duration READ duration NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
public:
    QueryTab(Workspace *ws, const QVariantMap &conn, const QString &host, int port,
             const QString &extra = {}, QObject *parent = nullptr);
    bool busy() const override;
    QString editorText() const { return m_editor; }
    void setEditorText(const QString &text);
    QObject *gridObject() const;
    GridModel *grid() const { return m_grid; }
    QString message() const { return m_message; }
    qint64 duration() const { return m_duration; }
    QVariantList history() const { return m_history; }
    QString saveExtra() const override;
    void shutdown() override;

    Q_INVOKABLE void run();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearResult();
    Q_INVOKABLE bool needsConfirm() const;
    Q_INVOKABLE QString guardMessage() const;
    Q_INVOKABLE void loadHistory();
    Q_INVOKABLE void useHistory(int index);
    Q_INVOKABLE void deleteHistory(int index);

signals:
    void changed();
    void textChanged();
    void historyChanged();

private:
    void connectSession();
    Workspace *m_ws;
    DbSession *m_session;
    GridModel *m_grid;
    QVariantMap m_conn;
    QString m_host;
    int m_port;
    QString m_editor;
    QString m_message;
    qint64 m_duration = 0;
    bool m_connected = false;
    QTimer *m_saveTimer = nullptr;
    QVariantList m_history;
};

class DesignTab : public Tab {
    Q_OBJECT
    Q_PROPERTY(QString refName READ refName CONSTANT)
    Q_PROPERTY(QString objectKind READ objectKind CONSTANT)
    Q_PROPERTY(QObject *fields READ fieldsObject CONSTANT)
    Q_PROPERTY(QObject *indexes READ indexesObject CONSTANT)
    Q_PROPERTY(QObject *foreignKeys READ fksObject CONSTANT)
    Q_PROPERTY(QString ddlText READ ddlText NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    DesignTab(Workspace *ws, const QString &kind, const QString &name, const QVariantMap &conn,
              const QString &host, int port, QObject *parent = nullptr);
    bool busy() const override;
    QString refName() const { return m_name; }
    QString objectKind() const { return m_objKind; }
    QObject *fieldsObject() const;
    QObject *indexesObject() const;
    QObject *fksObject() const;
    GridModel *fields() const { return m_fields; }
    GridModel *indexes() const { return m_indexes; }
    GridModel *fks() const { return m_fks; }
    QString ddlText() const { return m_ddl; }
    QString error() const { return m_error; }
    QString saveRef() const override { return m_objKind + '|' + m_name; }
    void shutdown() override;
    Q_INVOKABLE void reload();

signals:
    void changed();

private:
    void connectSession();
    Workspace *m_ws;
    DbSession *m_session;
    GridModel *m_fields;
    GridModel *m_indexes;
    GridModel *m_fks;
    QString m_objKind;
    QString m_name;
    QString m_ddl;
    QString m_error;
    bool m_connected = false;
};

class TabModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Role { TitleRole = Qt::UserRole + 1, KindRole, BusyRole, TabRole };
    explicit TabModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &p = {}) const override { return p.isValid() ? 0 : m_tabs.size(); }
    QVariant data(const QModelIndex &i, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void add(Tab *tab);
    void removeAt(int index);
    void clear();
    Tab *at(int index) const { return index >= 0 && index < m_tabs.size() ? m_tabs[index] : nullptr; }
    Q_INVOKABLE QObject *tabAt(int index) const;
signals:
    void countChanged();
private:
    QList<Tab *> m_tabs;
};

class ObjectModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
public:
    enum Role { NameRole = Qt::UserRole + 1, KindRole, SectionRole };
    explicit ObjectModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &p = {}) const override { return p.isValid() ? 0 : m_rows.size(); }
    QVariant data(const QModelIndex &i, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setObjects(const QVariantList &objects, const QStringList &warnings);
    void setFilter(const QString &text);
    QStringList warnings() const { return m_warnings; }
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE QString kindAt(int row) const;
signals:
    void changed();
private:
    QVariantList m_all;
    QVariantList m_rows;
    QStringList m_warnings;
    QString m_filter;
};

// ---- workspace --------------------------------------------------------------
class Workspace : public QObject {
    Q_OBJECT
    Q_PROPERTY(int connId READ connId WRITE setConnId NOTIFY connIdChanged)
    Q_PROPERTY(QObject *store READ store WRITE setStore NOTIFY storeChanged)
    Q_PROPERTY(QObject *tunnels READ tunnels WRITE setTunnels NOTIFY storeChanged)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool opening READ opening NOTIFY changed)
    Q_PROPERTY(bool opened READ opened NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(QObject *objects READ objectsObject CONSTANT)
    Q_PROPERTY(QObject *tabModel READ tabModelObject CONSTANT)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY changed)
public:
    explicit Workspace(QObject *parent = nullptr);
    ~Workspace() override;

    int connId() const { return m_connId; }
    void setConnId(int id);
    QObject *store() const { return m_store; }
    void setStore(QObject *store);
    QObject *tunnels() const { return m_tunnels; }
    void setTunnels(QObject *t);
    QString name() const { return m_name; }
    QString detail() const { return m_detail; }
    QString status() const { return m_status; }
    QString error() const { return m_error; }
    bool opening() const { return m_opening; }
    bool opened() const { return m_opened; }
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int i);
    QObject *objectsObject() const;
    QObject *tabModelObject() const;
    QStringList warnings() const { return m_warnings; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void trustTunnel(const QString &fingerprint);
    Q_INVOKABLE void openTable(const QString &table, const QString &objKind = QStringLiteral("table"));
    Q_INVOKABLE void openDesign(const QString &kind, const QString &name);
    Q_INVOKABLE void openQuery();
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void setFilter(const QString &text);
    Q_INVOKABLE void saveTabs();

signals:
    void connIdChanged();
    void storeChanged();
    void changed();
    void currentChanged();
    void tunnelPrompt(int code, const QString &message, const QString &fingerprint);

private:
    void beginTunnelOrConnect();
    void connectDatabase(const QString &host, int port);
    void restoreTabs();
    void persistTabs();
    void addTab(Tab *tab, bool persist);
    void setError(const QString &e);
    void updateDetail();

    int m_connId = 0;
    QObject *m_store = nullptr;
    QObject *m_tunnels = nullptr;
    QVariantMap m_conn;
    ObjectModel *m_objects;
    TabModel *m_tabs;
    DbSession *m_session;
    QString m_name;
    QString m_detail;
    QString m_status;
    QString m_error;
    QStringList m_warnings;
    QString m_pendingFp;
    bool m_opening = false;
    bool m_opened = false;
    bool m_tunnelHeld = false;
    bool m_restored = false;
    int m_current = -1;
    int m_connectHostPort = 0;
    QString m_connectHost;
};
