#pragma once
#include <QObject>
#include <QVariantMap>

// Android integration (dynamic accent, system bar insets/appearance). No-ops on desktop.
class Platform : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap insets READ insets NOTIFY insetsChanged)  // dp: top,bottom,left,right
    Q_PROPERTY(QString debugStart READ debugStart CONSTANT)           // desktop preview only
public:
    explicit Platform(QObject *parent = nullptr);
    QVariantMap insets() const { return m_insets; }
    QString debugStart() const { return qEnvironmentVariable("NAWI_START"); }
    Q_INVOKABLE QString accentHex(bool dark) const;  // "" when unavailable
    Q_INVOKABLE void setLightBars(bool light) const;
signals:
    void insetsChanged();
private:
    void refresh();
    QVariantMap m_insets{{"top", 0}, {"bottom", 0}, {"left", 0}, {"right", 0}};
};
