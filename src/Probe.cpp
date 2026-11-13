#include "Probe.h"
#include <mysql.h>
#include <openssl/crypto.h>

extern "C" const char *ssh_bridge_version();
extern "C" int ssh_bridge_selftest();

QString Probe::mariadbClientVersion() { return QString::fromUtf8(mysql_get_client_info()); }
QString Probe::opensslVersion() { return QString::fromUtf8(OpenSSL_version(OPENSSL_VERSION)); }

QString Probe::sshBridgeVersion()
{
    return QString::fromUtf8(ssh_bridge_version()) + (ssh_bridge_selftest() == 1 ? " ok" : " FAIL");
}

QString Probe::serverVersion(const QString &host, unsigned port, const QString &user,
                             const QString &pass, const QString &db, bool ssl)
{
    MYSQL *m = mysql_init(nullptr);
    unsigned timeout = 10;
    mysql_options(m, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    if (ssl) { // ponytail: TLS without cert verification (PLAN "SSL On"); add verify option if needed
        my_bool on = 1, verify = 0;
        mysql_options(m, MYSQL_OPT_SSL_ENFORCE, &on);
        mysql_options(m, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);
    }
    if (!mysql_real_connect(m, host.toUtf8().constData(), user.toUtf8().constData(),
                            pass.toUtf8().constData(), db.toUtf8().constData(), port, nullptr, 0)) {
        QString e = "ERROR: " + QString::fromUtf8(mysql_error(m));
        mysql_close(m);
        return e;
    }
    QString v = QString::fromUtf8(mysql_get_server_info(m));
    mysql_close(m);
    return v;
}
