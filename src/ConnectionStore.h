#pragma once
#include <QAbstractListModel>
#include <QVariantMap>

// Saved server connections. Model roles = non-secret fields. Secrets (password, sshSecret,
// httpToken) are encrypted via Vault and only returned by get().
// mode: 0 direct, 1 SSH tunnel, 2 HTTP (PHP agent). sshAuth: 0 password, 1 private key.
class ConnectionStore : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool testing READ testing NOTIFY testingChanged)
public:
    explicit ConnectionStore(QObject *parent = nullptr);
    int rowCount(const QModelIndex &p = {}) const override { return p.isValid() ? 0 : m_rows.size(); }
    QVariant data(const QModelIndex &i, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool testing() const { return m_testing; }

    // Returns "" on success or a user-facing error.
    Q_INVOKABLE QString save(const QVariantMap &c);
    Q_INVOKABLE void remove(int id);
    Q_INVOKABLE QVariantMap get(int id) const;   // includes decrypted secrets
    Q_INVOKABLE void testConnection(const QVariantMap &c);
    static QString validate(const QVariantMap &c); // "" when valid
signals:
    void countChanged();
    void testingChanged();
    void testFinished(bool ok, const QString &message);
private:
    void reload();
    QList<QVariantMap> m_rows;
    QStringList m_keys;
    bool m_testing = false;
};
