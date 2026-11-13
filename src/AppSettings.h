#pragma once
#include <QObject>

class AppSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY changed)          // system | light | dark
    Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY changed)      // data grid rows per page
    Q_PROPERTY(int rowLimit READ rowLimit WRITE setRowLimit NOTIFY changed)      // SQL editor max rows
    Q_PROPERTY(int queryTimeout READ queryTimeout WRITE setQueryTimeout NOTIFY changed) // seconds
    Q_PROPERTY(bool onboarded READ onboarded WRITE setOnboarded NOTIFY changed)
public:
    explicit AppSettings(QObject *parent = nullptr);
    QString theme() const { return m_theme; }
    int pageSize() const { return m_pageSize; }
    int rowLimit() const { return m_rowLimit; }
    int queryTimeout() const { return m_timeout; }
    bool onboarded() const { return m_onboarded; }
    void setTheme(const QString &v);
    void setPageSize(int v);
    void setRowLimit(int v);
    void setQueryTimeout(int v);
    void setOnboarded(bool v);
signals:
    void changed();
private:
    void store(const char *key, const QString &v);
    QString m_theme = "system";
    int m_pageSize = 100, m_rowLimit = 100, m_timeout = 300;
    bool m_onboarded = false;
};
