#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>

// AES-256-GCM envelope shared with agent/tunnel.php.
// Key = HKDF-SHA256(token, info "nawidbman-v1"), 32 bytes. AAD differs per direction so a
// request envelope cannot be replayed back as a response.
namespace HttpCrypto {

// AAD markers; must match agent/tunnel.php byte for byte.
constexpr const char *kAadReq = "nawidbman-v1/req";
constexpr const char *kAadRes = "nawidbman-v1/res";

QByteArray key(const QString &token);

// {"v":1,"n":b64(nonce),"d":b64(ciphertext),"t":b64(tag)} or empty object on failure.
QJsonObject seal(const QByteArray &key, const QJsonObject &body, const char *aad);
bool open(const QByteArray &key, const QJsonObject &envelope, const char *aad, QJsonObject *out);

}
