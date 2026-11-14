#pragma once
#include <QAbstractListModel>

// Read-only rows for QML grids. One role returns the whole row as a list.
class GridModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int rowCount READ rowCount NOTIFY reset)
    Q_PROPERTY(int columnCount READ columnCount NOTIFY reset)
    Q_PROPERTY(QStringList headers READ headers NOTIFY reset)
    Q_PROPERTY(QStringList types READ types NOTIFY reset)
    Q_PROPERTY(QVariantList widths READ widths NOTIFY reset)
public:
    enum Role { RowRole = Qt::UserRole + 1 };

    explicit GridModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &p = {}) const override { return p.isValid() ? 0 : m_rows.size(); }
    QVariant data(const QModelIndex &i, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setResult(const QVariantMap &result);   // map from DbSession
    void clear();
    void setRows(const QStringList &headers, const QVariantList &rows, const QStringList &types = {});

    Q_INVOKABLE QVariant cell(int row, int col) const;
    Q_INVOKABLE bool isNull(int row, int col) const;
    Q_INVOKABLE int widthFor(int col) const;
    QStringList headers() const { return m_headers; }
    QStringList types() const { return m_types; }
    QVariantList widths() const { return m_widths; }
    int columnCount() const { return m_headers.size(); }
    const QList<QVariantList> &rows() const { return m_rows; }
    QVariantList row(int r) const { return r >= 0 && r < m_rows.size() ? m_rows[r] : QVariantList(); }

signals:
    void reset();

private:
    void recomputeWidths();
    QStringList m_headers;
    QStringList m_types;
    QVariantList m_widths;
    QList<QVariantList> m_rows;
};
