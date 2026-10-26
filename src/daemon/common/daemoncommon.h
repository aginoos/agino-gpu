// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Small helpers shared by agino-gpud, agino-gpu-apply and agino-gpu-integrated-remove.
// Header-only on purpose: the library contract does not cover raw sysfs control writes.

#include "agino-gpu/root.h"
#include "agino-gpu/types.h"

#include <QByteArray>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace AginoGpu {

inline const QString kAsusDgpuDisablePath = QStringLiteral("/sys/devices/platform/asus-nb-wmi/dgpu_disable");
inline const QString kPciRescanPath = QStringLiteral("/sys/bus/pci/rescan");
// Created by agino-gpu-apply when an integrated boot has to bring the discrete GPU back; tells
// agino-gpu-integrated-remove to leave it alone for the rest of this boot.
inline const QString kKeepDgpuPath = QStringLiteral("/run/agino-gpu/keep-dgpu");

inline int nvidiaGpuCount(const Hardware &hw)
{
    int n = 0;
    for (const Gpu &g : hw.gpus) {
        if (g.vendor == Vendor::Nvidia)
            ++n;
    }
    return n;
}

// Root::writeFile() renames a temporary file over the target, which cannot work on sysfs;
// control files need a plain write.
inline bool writeSysfsValue(const Root &root, const QString &absolute, const QByteArray &value,
                            QString *error = nullptr)
{
    QFile f(root.path(absolute));
    if (!f.open(QIODevice::WriteOnly) || f.write(value) != value.size() || !f.flush()) {
        if (error)
            *error = QStringLiteral("cannot write %1: %2").arg(f.fileName(), f.errorString());
        return false;
    }
    f.close();
    return true;
}

inline bool isPciAddress(const QString &s)
{
    static const QRegularExpression re(
        QStringLiteral("^[0-9a-f]{4,8}:[0-9a-f]{2}:[0-9a-f]{2}\\.[0-7]$"));
    return re.match(s).hasMatch();
}

// Removes the other functions of the slot first, then the GPU itself. Returns the addresses
// that were removed; failures are appended to `errors`.
inline QStringList removeDiscreteGpu(const Root &root, const Gpu &gpu, QStringList *errors = nullptr)
{
    QStringList order = gpu.functions;
    order.append(gpu.pciAddress);
    QStringList removed;
    for (const QString &addr : std::as_const(order)) {
        if (!isPciAddress(addr))
            continue;
        const QString path = QStringLiteral("/sys/bus/pci/devices/%1/remove").arg(addr);
        if (!root.exists(path))
            continue;
        QString err;
        if (writeSysfsValue(root, path, "1", &err))
            removed.append(addr);
        else if (errors)
            errors->append(err);
    }
    return removed;
}

} // namespace AginoGpu
