// SPDX-License-Identifier: GPL-3.0-or-later
// agino-gpud: system-bus service org.agino.Gpu1 (design.md §6.2). Started by D-Bus activation,
// exits after 60 s without incoming calls.

#include "agino-gpu/root.h"
#include "dbustypes.h"
#include "gpuservice.h"

#include <QCoreApplication>
#include <QDBusConnection>

using namespace AginoGpu;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("agino-gpud"));

    registerDBusTypes();

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        qCritical("cannot connect to the system bus");
        return 1;
    }

    constexpr int kIdleSeconds = 60;
    GpuService service(Root::fromEnvironment(), kIdleSeconds);
    if (!bus.registerObject(QStringLiteral("/org/agino/Gpu1"), &service, QDBusConnection::ExportAllContents)) {
        qCritical("cannot register the object: %s", qPrintable(bus.lastError().message()));
        return 1;
    }
    if (!bus.registerService(QStringLiteral("org.agino.Gpu1"))) {
        qCritical("cannot own org.agino.Gpu1: %s", qPrintable(bus.lastError().message()));
        return 1;
    }
    qInfo("agino-gpud started");
    return app.exec();
}
