#pragma once
#include <QString>

namespace Vault {
// Android: AES-256-GCM with an Android Keystore key. Returns a null QString on failure.
// Empty input maps to empty output (no secret). Desktop (tests/preview): NOT encrypted, base64 only.
QString encrypt(const QString &plain);
QString decrypt(const QString &enc);
}
