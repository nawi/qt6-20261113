#pragma once
#include <QObject>
#include <QVariantMap>

// Android integration (dynamic accent, system bar insets/appearance). No-ops on desktop.
class Platform : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap insets READ insets NOTIFY insetsChanged)  // dp: top,bottom,left,right,ime
    Q_PROPERTY(QString debugStart READ debugStart CONSTANT)           // desktop preview only
public:
    explicit Platform(QObject *parent = nullptr);
    QVariantMap insets() const { return m_insets; }
    QString debugStart() const { return qEnvironmentVariable("NAWI_START"); }
    Q_INVOKABLE QString accentHex(bool dark) const;  // "" when unavailable
    Q_INVOKABLE void setLightBars(bool light) const;
    // Saves text to the app cache and opens the Android share sheet. Returns false on desktop/failure.
    Q_INVOKABLE bool shareText(const QString &fileName, const QString &text, const QString &mime) const;
signals:
    void insetsChanged();
private:
    void refresh();
    QVariantMap m_insets{{"top", 0}, {"bottom", 0}, {"left", 0}, {"right", 0}, {"ime", 0}};
};
