#include "AppSettings.h"
#include "Db.h"
#include <QtGlobal>

AppSettings::AppSettings(QObject *parent) : QObject(parent)
{
    m_theme = Db::get("theme", "system");
    if (m_theme != "light" && m_theme != "dark") m_theme = "system";
    m_pageSize = qBound(10, Db::get("pageSize", "100").toInt(), 1000);
    m_rowLimit = qBound(1, Db::get("rowLimit", "100").toInt(), 10000);
    m_timeout = qBound(5, Db::get("queryTimeout", "300").toInt(), 3600);
    m_onboarded = Db::get("onboarded") == "1";
}

void AppSettings::store(const char *key, const QString &v) { Db::set(key, v); emit changed(); }

void AppSettings::setTheme(const QString &v)
{
    const QString t = (v == "light" || v == "dark") ? v : "system";
    if (t != m_theme) { m_theme = t; store("theme", t); }
}
void AppSettings::setPageSize(int v)
{
    v = qBound(10, v, 1000);
    if (v != m_pageSize) { m_pageSize = v; store("pageSize", QString::number(v)); }
}
void AppSettings::setRowLimit(int v)
{
    v = qBound(1, v, 10000);
    if (v != m_rowLimit) { m_rowLimit = v; store("rowLimit", QString::number(v)); }
}
void AppSettings::setQueryTimeout(int v)
{
    v = qBound(5, v, 3600);
    if (v != m_timeout) { m_timeout = v; store("queryTimeout", QString::number(v)); }
}
void AppSettings::setOnboarded(bool v)
{
    if (v != m_onboarded) { m_onboarded = v; store("onboarded", v ? "1" : "0"); }
}
