#include "Platform.h"
#include <QColor>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

#ifdef Q_OS_ANDROID
#include <QJniObject>
static QJniObject ctx() { return QJniObject(QNativeInterface::QAndroidApplication::context()); }
#endif

Platform::Platform(QObject *parent) : QObject(parent)
{
#ifdef Q_OS_ANDROID
    // ponytail: 400ms poll is the cheapest way to track bars/rotation/cutout; switch to
    // OnApplyWindowInsetsListener via JNI callback if battery or latency matters.
    auto *t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &Platform::refresh);
    t->start(400);
    refresh();
#endif
}

void Platform::refresh()
{
#ifdef Q_OS_ANDROID
    const QJniObject s = QJniObject::callStaticObjectMethod("com/nawi/dbman/Vault", "insets",
            "(Landroid/content/Context;)Ljava/lang/String;", ctx().object());
    const QStringList p = s.isValid() ? s.toString().split(',') : QStringList();
    if (p.size() < 4) return;
    const qreal d = QGuiApplication::primaryScreen()->devicePixelRatio();
    const QVariantMap m{{"top", p[0].toInt() / d}, {"bottom", p[1].toInt() / d},
                        {"left", p[2].toInt() / d}, {"right", p[3].toInt() / d},
                        {"ime", p.size() > 4 ? p[4].toInt() / d : 0.0}};
    if (m != m_insets) { m_insets = m; emit insetsChanged(); }
#endif
}

QString Platform::accentHex(bool dark) const
{
#ifdef Q_OS_ANDROID
    const jint c = QJniObject::callStaticMethod<jint>("com/nawi/dbman/Vault", "accent",
            "(Landroid/content/Context;Z)I", ctx().object(), jboolean(dark));
    return c == 0 ? QString() : QColor::fromRgba(static_cast<QRgb>(c)).name();
#else
    Q_UNUSED(dark);
    return {};
#endif
}

void Platform::setLightBars(bool light) const
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("com/nawi/dbman/Vault", "setLightBars",
            "(Landroid/content/Context;Z)V", ctx().object(), jboolean(light));
#else
    Q_UNUSED(light);
#endif
}
