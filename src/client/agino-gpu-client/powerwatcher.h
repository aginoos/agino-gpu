// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

namespace AginoGpu {

// UPower's OnBattery (org.freedesktop.UPower on the system bus), followed through
// PropertiesChanged. Without UPower: never on battery.
class PowerWatcher : public QObject
{
    Q_OBJECT
public:
    explicit PowerWatcher(QObject *parent = nullptr);

    bool onBattery() const { return m_onBattery; }

Q_SIGNALS:
    void onBatteryChanged(bool onBattery);

private Q_SLOTS:
    void propertiesChanged(const QString &interface, const QVariantMap &changed,
                           const QStringList &invalidated);

private:
    void fetch();
    void set(bool onBattery);

    bool m_onBattery = false;
};

// Logout / reboot through the DDE session manager, which shows its own confirmation and
// handles unsaved work (org.deepin.dde.SessionManager1 RequestLogout / RequestReboot).
namespace Session {
void requestLogout();
void requestReboot();
} // namespace Session

} // namespace AginoGpu
