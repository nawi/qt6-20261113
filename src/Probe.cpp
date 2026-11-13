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
    // Connector/C 3.4 verifies the server cert by default, which also rejects plaintext servers.
    // Policy (PLAN): SSL On = encrypted, cert NOT verified; SSL Off = plaintext allowed.
    // ponytail: no cert verification ever; add a "Verify certificate" option + CA field if needed.
    my_bool verify = 0, enforce = ssl ? 1 : 0;
    mysql_optionsv(m, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);
    mysql_optionsv(m, MYSQL_OPT_SSL_ENFORCE, &enforce);
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
