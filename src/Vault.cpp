#include "Vault.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QCoreApplication>

static QString call(const char *method, const QString &in)
{
    QJniObject r = QJniObject::callStaticObjectMethod("com/nawi/dbman/Vault", method,
            "(Ljava/lang/String;)Ljava/lang/String;", QJniObject::fromString(in).object<jstring>());
    return r.isValid() ? r.toString() : QString();
}
QString Vault::encrypt(const QString &plain) { return plain.isEmpty() ? QString("") : call("encrypt", plain); }
QString Vault::decrypt(const QString &enc) { return enc.isEmpty() ? QString("") : call("decrypt", enc); }
#else
// ponytail: desktop has no keystore; only used by unit tests and UI preview. Add libsecret if a desktop build ships.
QString Vault::encrypt(const QString &plain)
{
    return plain.isEmpty() ? QString("") : "plain:" + QString::fromLatin1(plain.toUtf8().toBase64());
}
QString Vault::decrypt(const QString &enc)
{
    if (enc.isEmpty()) return QString("");
    return enc.startsWith("plain:") ? QString::fromUtf8(QByteArray::fromBase64(enc.mid(6).toLatin1())) : QString();
}
#endif
