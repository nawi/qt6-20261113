#include "HttpAgent.h"
#include "HttpCrypto.h"
#include <QDateTime>
#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QTimer>

bool HttpAgent::post(const QString &url, const QString &token, const QJsonObject &body, bool encrypt,
                     int timeoutSec, QJsonObject *reply, QString *error)
{
    QJsonObject payload = body;
    const QByteArray key = encrypt ? HttpCrypto::key(token) : QByteArray();
    if (encrypt) {
        payload["t"] = qint64(QDateTime::currentSecsSinceEpoch());   // anti-replay
        payload = HttpCrypto::seal(key, payload, HttpCrypto::kAadReq);
        if (payload.isEmpty()) {
            if (error) *error = QObject::tr("Cannot encrypt request.");
            return false;
        }
    }

    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("X-Nawi-Token", token.toUtf8());
    req.setTransferTimeout(timeoutSec * 1000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *r = nam.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray data = r->readAll();
    const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError ne = r->error();
    const QString es = r->errorString();
    r->deleteLater();

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    // The agent reports 4xx/5xx with a JSON error body; prefer its message over Qt's generic one.
    if (pe.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject o = doc.object();
        if (status >= 400 || ne != QNetworkReply::NoError) {
            const QString msg = o.value("error").toObject().value("msg").toString();
            if (error) *error = QObject::tr("Agent error (HTTP %1): %2").arg(status).arg(msg.isEmpty() ? es : msg);
            return false;
        }
        if (o.contains("v")) {   // encrypted reply
            QJsonObject inner;
            if (!HttpCrypto::open(key, o, HttpCrypto::kAadRes, &inner)) {
                if (error) *error = QObject::tr("Agent reply could not be decrypted (token mismatch?).");
                return false;
            }
            o = inner;
        }
        *reply = o;
        return true;
    }
    if (error) {
        if (ne != QNetworkReply::NoError)
            *error = QObject::tr("Agent unreachable: %1").arg(es);
        else
            *error = QObject::tr("Agent returned an invalid response (HTTP %1). Check the URL.").arg(status);
    }
    return false;
}
