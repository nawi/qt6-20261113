#include <QtTest>
#include <QSqlQuery>
#include "AppSettings.h"
#include "ConnectionStore.h"
#include "Db.h"
#include "Vault.h"

class TstStorage : public QObject {
    Q_OBJECT
    static QVariantMap direct() {
        return {{"name", "A"}, {"mode", 0}, {"host", "h"}, {"port", 3306}, {"user", "u"}, {"password", "s3cret"},
                {"database", "d"}, {"ssl", true}, {"color", "#FF3B30"}};
    }
private slots:
    void initTestCase() { QVERIFY(Db::open(":memory:")); }

    void vaultRoundTrip() {
        QCOMPARE(Vault::decrypt(Vault::encrypt(QString::fromUtf8("pässwörd'\"; DROP"))), QString::fromUtf8("pässwörd'\"; DROP"));
        QCOMPARE(Vault::encrypt(""), QString(""));
        QVERIFY(Vault::decrypt("garbage").isNull());
    }
    void settingsDefaultsAndClamp() {
        AppSettings s;
        QCOMPARE(s.pageSize(), 100); QCOMPARE(s.rowLimit(), 100); QCOMPARE(s.queryTimeout(), 300);
        QCOMPARE(s.theme(), QString("system")); QVERIFY(!s.onboarded());
        s.setQueryTimeout(-5); QCOMPARE(s.queryTimeout(), 5);
        s.setPageSize(999999); QCOMPARE(s.pageSize(), 1000);
        s.setTheme("neon"); QCOMPARE(s.theme(), QString("system"));
        s.setTheme("dark"); s.setOnboarded(true);
        AppSettings again; // persisted
        QCOMPARE(again.theme(), QString("dark")); QVERIFY(again.onboarded()); QCOMPARE(again.queryTimeout(), 5);
    }
    void validation() {
        auto c = direct();
        QVERIFY(ConnectionStore::validate(c).isEmpty());
        auto bad = c; bad["database"] = " "; QVERIFY(!ConnectionStore::validate(bad).isEmpty());   // db required
        bad = c; bad["port"] = 70000; QVERIFY(!ConnectionStore::validate(bad).isEmpty());
        bad = c; bad["mode"] = 1; QVERIFY(!ConnectionStore::validate(bad).isEmpty());              // ssh fields missing
        bad = c; bad["mode"] = 2; bad["httpUrl"] = "ftp://x"; QVERIFY(!ConnectionStore::validate(bad).isEmpty());
        bad = c; bad["mode"] = 1; bad["sshHost"] = "b"; bad["sshPort"] = 22; bad["sshUser"] = "x";
        bad["sshAuth"] = 1; bad["sshSecret"] = "not a key"; QVERIFY(!ConnectionStore::validate(bad).isEmpty());
        bad["sshSecret"] = "-----BEGIN OPENSSH PRIVATE KEY-----\nabc"; QVERIFY(ConnectionStore::validate(bad).isEmpty());
    }
    void crud() {
        ConnectionStore st;
        { const QString e = st.save(direct()); QVERIFY2(e.isEmpty(), qPrintable(e)); }
        QCOMPARE(st.rowCount(), 1);
        const int id = st.data(st.index(0), Qt::UserRole).toInt();
        QVERIFY(id > 0);
        QVariantMap g = st.get(id);
        QCOMPARE(g["password"].toString(), QString("s3cret"));  // decrypted only via get()
        QCOMPARE(g["database"].toString(), QString("d"));
        // secrets never in the model roles
        QVERIFY(!st.roleNames().values().contains("password"));
        QSqlQuery q("SELECT password_enc FROM connections"); q.next();
        QVERIFY(!q.value(0).toString().contains("s3cret"));
        auto u = direct(); u["id"] = id; u["name"] = "B";
        QVERIFY(st.save(u).isEmpty());
        QCOMPARE(st.rowCount(), 1);
        QCOMPARE(st.get(id)["name"].toString(), QString("B"));
        QVERIFY(!st.save({{"name", ""}}).isEmpty());   // invalid rejected, nothing written
        QCOMPARE(st.rowCount(), 1);
        st.remove(id);
        QCOMPARE(st.rowCount(), 0);
    }
};
QTEST_MAIN(TstStorage)
#include "tst_storage.moc"
