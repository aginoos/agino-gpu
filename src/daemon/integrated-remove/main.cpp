// SPDX-License-Identifier: GPL-3.0-or-later
// Run from udev (82-agino-gpu-integrated.rules) with the kernel name of a PCI display or audio
// device. In integrated mode the discrete GPU and its functions are removed from the PCI bus
// before any driver binds (design.md §4.2). In every other mode this does nothing.

#include "agino-gpu/detect.h"
#include "agino-gpu/mode.h"
#include "agino-gpu/root.h"
#include "daemoncommon.h"

#include <QString>

#include <cstdio>

using namespace AginoGpu;

int main(int argc, char **argv)
{
    if (argc < 2)
        return 0;
    const QString addr = QString::fromLocal8Bit(argv[1]);
    if (!isPciAddress(addr))
        return 0;

    const Root root = Root::fromEnvironment();
    if (readConfiguredMode(root) != Mode::Integrated)
        return 0;

    const Hardware hw = detectHardware(root);
    const Gpu *dgpu = hw.discreteGpu();
    if (!dgpu)
        return 0;
    if (addr != dgpu->pciAddress && !dgpu->functions.contains(addr))
        return 0;

    QStringList errors;
    const QStringList removed = removeDiscreteGpu(root, *dgpu, &errors);
    for (const QString &a : removed)
        std::fprintf(stderr, "agino-gpu-integrated-remove: removed %s\n", qPrintable(a));
    for (const QString &e : std::as_const(errors))
        std::fprintf(stderr, "agino-gpu-integrated-remove: %s\n", qPrintable(e));

    if (hw.asusDgpuDisable || root.exists(kAsusDgpuDisablePath)) {
        QString err;
        if (!writeSysfsValue(root, kAsusDgpuDisablePath, "1", &err))
            std::fprintf(stderr, "agino-gpu-integrated-remove: %s\n", qPrintable(err));
    }
    return 0;
}
