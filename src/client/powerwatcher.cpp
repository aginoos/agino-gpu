// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu-client/powerwatcher.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

namespace AginoGpu {

namespace {

const QString kUPowerService = QStringLiteral("org.freedesktop.UPower");
const QString kUPowerPath = QStringLiteral("/org/freedesktop/UPower");
const QString kPropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kOnBattery = QStringLiteral("OnBattery");

const QString kSessionService = QStringLiteral("org.deepin.dde.SessionManager1");
const QString kSessionPath = QStringLiteral("/org/deepin/dde/SessionManager1");

void callSession(const QString &method)
{
    const QDBusMessage msg =
        QDBusMessage::createMethodCall(kSessionService, kSessionPath, kSessionService, method);
    QDBusConnection::sessionBus().asyncCall(msg);
}

} // namespace

PowerWatcher::PowerWatcher(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::systemBus().connect(kUPowerService, kUPowerPath, kPropertiesInterface,
                                         QStringLiteral("PropertiesChanged"), this,
                                         SLOT(propertiesChanged(QString, QVariantMap, QStringList)));
    fetch();
}

void PowerWatcher::fetch()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kUPowerService, kUPowerPath,
                                                      kPropertiesInterface, QStringLiteral("Get"));
    msg << kUPowerService << kOnBattery;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusVariant> reply(*w);
        if (reply.isValid())
            set(reply.value().variant().toBool());
    });
}

void PowerWatcher::propertiesChanged(const QString &interface, const QVariantMap &changed,
                                     const QStringList &invalidated)
{
    if (interface != kUPowerService)
        return;
    if (auto it = changed.constFind(kOnBattery); it != changed.cend())
        set(it->toBool());
    else if (invalidated.contains(kOnBattery))
        fetch();
}

void PowerWatcher::set(bool onBattery)
{
    if (onBattery == m_onBattery)
        return;
    m_onBattery = onBattery;
    Q_EMIT onBatteryChanged(onBattery);
}

namespace Session {

void requestLogout()
{
    callSession(QStringLiteral("RequestLogout"));
}

void requestReboot()
{
    callSession(QStringLiteral("RequestReboot"));
}

} // namespace Session

} // namespace AginoGpu
