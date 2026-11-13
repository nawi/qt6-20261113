#pragma once
#include <QString>

namespace Probe {
QString mariadbClientVersion();
QString opensslVersion();
QString sshBridgeVersion();
// Returns server version on success, otherwise "ERROR: <msg>". ssl=false disables TLS.
QString serverVersion(const QString &host, unsigned port, const QString &user,
                      const QString &pass, const QString &db, bool ssl);
}
