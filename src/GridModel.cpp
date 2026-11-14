#include "GridModel.h"
#include <QVariantMap>

GridModel::GridModel(QObject *parent) : QAbstractListModel(parent) {}

QHash<int, QByteArray> GridModel::roleNames() const
{
    return {{RowRole, "row"}};
}

QVariant GridModel::data(const QModelIndex &i, int role) const
{
    if (role != RowRole || !i.isValid() || i.row() >= m_rows.size()) return {};
    return m_rows[i.row()];
}

void GridModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_headers.clear();
    m_types.clear();
    m_widths.clear();
    endResetModel();
    emit reset();
}

void GridModel::setRows(const QStringList &headers, const QVariantList &rows, const QStringList &types)
{
    beginResetModel();
    m_headers = headers;
    m_types = types;
    m_rows.clear();
    for (const QVariant &r : rows) m_rows << r.toList();
    endResetModel();
    recomputeWidths();
    emit reset();
}

void GridModel::setResult(const QVariantMap &result)
{
    if (!result.value("ok").toBool()) { clear(); return; }
    QStringList headers;
    const QVariantList cols = result.value("columns").toList();
    for (const QVariant &c : cols) headers << c.toMap().value("name").toString();
    if (headers.isEmpty()) headers = result.value("columnNames").toStringList();
    QStringList types;
    for (const QVariant &c : cols) types << c.toMap().value("type").toString();
    setRows(headers, result.value("rows").toList(), types);
}

void GridModel::recomputeWidths()
{
    m_widths.clear();
    for (int c = 0; c < m_headers.size(); ++c) {
        int chars = m_headers[c].size();
        for (int r = 0; r < m_rows.size() && r < 30; ++r) {
            const QVariant &v = m_rows[r].value(c);
            const int n = v.isNull() ? 4 : v.toString().size();
            if (n > chars) chars = n;
        }
        m_widths << qBound(64, chars * 8 + 28, 240);
    }
}

QVariant GridModel::cell(int row, int col) const
{
    if (row < 0 || row >= m_rows.size() || col < 0 || col >= m_headers.size()) return {};
    return m_rows[row].value(col);
}

bool GridModel::isNull(int row, int col) const
{
    if (row < 0 || row >= m_rows.size()) return false;
    return m_rows[row].value(col).isNull();
}

int GridModel::widthFor(int col) const
{
    return col >= 0 && col < m_widths.size() ? m_widths[col].toInt() : 80;
}
