// SPDX-License-Identifier: GPL-3.0-or-later
#include "controlcenter.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>

namespace AginoGpu {

void openControlCenterGpuPage()
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.deepin.dde.ControlCenter1"), QStringLiteral("/org/deepin/dde/ControlCenter1"),
        QStringLiteral("org.deepin.dde.ControlCenter1"), QStringLiteral("ShowPage"));
    call << QStringLiteral("display/gpu");
    if (!QDBusConnection::sessionBus().send(call))
        qWarning() << "agino-gpu: cannot ask the control center to show the GPU page";
}

} // namespace AginoGpu
