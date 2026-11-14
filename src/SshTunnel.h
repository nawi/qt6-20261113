#pragma once
#include <QHash>
#include <QObject>
#include <QVariantMap>

// One SSH tunnel per saved connection (refcounted by tabs). The heavy lifting is in
// rust/ssh_bridge; this class keeps the UI thread free and stores the host key (TOFU).
class TunnelManager : public QObject {
    Q_OBJECT
public:
    enum ErrorCode {
        Ok = 0,
        Network = 1,
        AuthFailed = 2,
        HostKeyUnknown = 3,   // UI must ask the user, then call trustFingerprint()
        HostKeyMismatch = 4,  // never auto-trusted
        OtherError = 5,
        BadKey = 6,
    };
    Q_ENUM(ErrorCode)

    explicit TunnelManager(QObject *parent = nullptr);
    ~TunnelManager() override;

    // Starts (or reuses) a tunnel for a saved connection id. Loads secrets via ConnectionStore.
    Q_INVOKABLE void acquire(int connectionId);
    Q_INVOKABLE void release(int connectionId);
    Q_INVOKABLE void trustFingerprint(int connectionId, const QString &fingerprint);

signals:
    void ready(int connectionId, const QString &host, int port);
    void failed(int connectionId, int code, const QString &message, const QString &fingerprint);

private:
    struct Entry {
        int refs = 0;
        quint64 handle = 0;
        quint16 port = 0;
        bool starting = false;
        QString fingerprint;
    };
    void start(int id);
    void stop(int id);
    void finish(int id);
    QHash<int, Entry> m_entries;
};

extern "C" {
unsigned long long ssh_bridge_connect(const char *host, unsigned short port, const char *user,
                                      int authKind, const char *secret, const char *expectedFp,
                                      const char *dbHost, unsigned short dbPort, int timeoutSec,
                                      unsigned short *localPortOut, char *fpOut, int fpLen,
                                      int *rcOut, char *errOut, int errLen);
void ssh_bridge_disconnect(unsigned long long id);
}
