#pragma once
#include <QJsonObject>
#include <QString>

// Blocking JSON POST to the PHP tunnel agent. Call from a worker thread only.
namespace HttpAgent {
// Returns false with *error on transport/HTTP/JSON failure. On success *reply is the parsed body
// (which may itself carry {"ok":false,"error":{code,msg}} for database errors).
bool post(const QString &url, const QString &token, const QJsonObject &body, int timeoutSec,
          QJsonObject *reply, QString *error);
}
