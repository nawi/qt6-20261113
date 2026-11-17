#pragma once
#include <QJsonObject>
#include <QString>

// Blocking JSON POST to the PHP tunnel agent. Call from a worker thread only.
namespace HttpAgent {
// encrypt=true wraps the body in an AES-256-GCM envelope (and decrypts the reply when it is
// one); false sends plain JSON for older agents. Returns false with *error on transport,
// HTTP or crypto failure. On success *reply is the parsed body (which may itself carry
// {"ok":false,"error":{code,msg}} for database errors).
bool post(const QString &url, const QString &token, const QJsonObject &body, bool encrypt,
          int timeoutSec, QJsonObject *reply, QString *error);
}
