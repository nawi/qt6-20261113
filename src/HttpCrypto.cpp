#include "HttpCrypto.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QMessageAuthenticationCode>
#include <QString>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace {
QByteArray hmac(const QByteArray &key, const QByteArray &data)
{
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
}

QByteArray b64(const unsigned char *p, int n)
{
    return QByteArray(reinterpret_cast<const char *>(p), n).toBase64();
}
}

QByteArray HttpCrypto::key(const QString &token)
{
    // HKDF-SHA256: extract with an empty salt (== HashLen zero bytes), expand one block.
    const QByteArray prk = hmac(QByteArray(32, '\0'), token.toUtf8());
    return hmac(prk, QByteArrayLiteral("nawidbman-v1") + char(0x01));
}

QJsonObject HttpCrypto::seal(const QByteArray &key, const QJsonObject &body, const char *aad)
{
    if (key.size() != 32) return {};
    const QByteArray plain = QJsonDocument(body).toJson(QJsonDocument::Compact);
    unsigned char nonce[12];
    if (RAND_bytes(nonce, sizeof nonce) != 1) return {};

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    QByteArray ct(plain.size() + 16, '\0');
    unsigned char tag[16];
    int outl = 0, fin = 0;
    const bool ok =
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, int(sizeof nonce), nullptr) == 1
        && EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                              reinterpret_cast<const unsigned char *>(key.constData()), nonce) == 1
        && EVP_EncryptUpdate(ctx, nullptr, &outl, reinterpret_cast<const unsigned char *>(aad),
                             int(qstrlen(aad))) == 1
        && EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(ct.data()), &outl,
                             reinterpret_cast<const unsigned char *>(plain.constData()),
                             plain.size()) == 1
        && EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(ct.data()) + outl, &fin) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, int(sizeof tag), tag) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};
    ct.resize(outl + fin);
    return {{"v", 1},
            {"n", QString::fromLatin1(b64(nonce, sizeof nonce))},
            {"d", QString::fromLatin1(ct.toBase64())},
            {"t", QString::fromLatin1(b64(tag, sizeof tag))}};
}

bool HttpCrypto::open(const QByteArray &key, const QJsonObject &envelope, const char *aad,
                      QJsonObject *out)
{
    if (key.size() != 32 || envelope.value("v").toInt() != 1) return false;
    const QByteArray nonce = QByteArray::fromBase64(envelope.value("n").toString().toLatin1());
    const QByteArray ct = QByteArray::fromBase64(envelope.value("d").toString().toLatin1());
    const QByteArray tag = QByteArray::fromBase64(envelope.value("t").toString().toLatin1());
    if (nonce.size() != 12 || tag.size() != 16) return false;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    QByteArray pt(ct.size() + 16, '\0');
    int outl = 0, fin = 0;
    const bool ok =
        EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) == 1
        && EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                              reinterpret_cast<const unsigned char *>(key.constData()),
                              reinterpret_cast<const unsigned char *>(nonce.constData())) == 1
        && EVP_DecryptUpdate(ctx, nullptr, &outl, reinterpret_cast<const unsigned char *>(aad),
                             int(qstrlen(aad))) == 1
        && EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char *>(pt.data()), &outl,
                             reinterpret_cast<const unsigned char *>(ct.constData()),
                             ct.size()) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
                               const_cast<unsigned char *>(
                                   reinterpret_cast<const unsigned char *>(tag.constData()))) == 1
        && EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(pt.data()) + outl, &fin) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return false;
    pt.resize(outl + fin);
    const QJsonDocument doc = QJsonDocument::fromJson(pt);
    if (!doc.isObject()) return false;
    *out = doc.object();
    return true;
}
