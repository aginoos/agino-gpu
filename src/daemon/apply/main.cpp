// SPDX-License-Identifier: GPL-3.0-or-later
// agino-gpu-apply: oneshot at boot, before the display manager (design.md §3.2 rule 4, §6.1).
// Makes the saved mode real for this boot and records what is active. Never blocks the boot:
// always exits 0.

#include "agino-gpu/configgen.h"
#include "agino-gpu/detect.h"
#include "agino-gpu/mode.h"
#include "agino-gpu/offloadenv.h"
#include "agino-gpu/root.h"
#include "daemoncommon.h"

#include <QString>
#include <QThread>

#include <cstdio>

using namespace AginoGpu;

namespace {

void say(const QString &line)
{
    std::fprintf(stdout, "agino-gpu-apply: %s\n", qPrintable(line));
    std::fflush(stdout);
}

// The ASUS firmware keeps the dGPU powered off across reboots once dgpu_disable=1; any mode
// that needs the GPU must power it on and rescan before we look at the hardware.
void wakeAsusDgpu(const Root &root)
{
    const auto value = root.readTrimmed(kAsusDgpuDisablePath);
    if (!value || *value != QLatin1String("1"))
        return;
    QString err;
    if (!writeSysfsValue(root, kAsusDgpuDisablePath, "0", &err)) {
        say(err);
        return;
    }
    say(QStringLiteral("dgpu_disable 1 -> 0"));
    QThread::msleep(1000);
    if (!writeSysfsValue(root, kPciRescanPath, "1", &err))
        say(err);
    else
        say(QStringLiteral("PCI rescan requested"));
}

void recordFailure(const Root &root, const Hardware &hw, const QString &reason)
{
    say(QStringLiteral("FAILURE: %1; falling back to hybrid").arg(reason));
    QString err;
    if (!root.writeFile(QString::fromLatin1(kLastFailurePath), (reason + QLatin1Char('\n')).toUtf8(), 0644, &err))
        say(err);
    if (!writeConfiguredMode(root, Mode::Hybrid, &err))
        say(err);
    if (!applyGeneratedFiles(root, hw, Mode::Hybrid, &err))
        say(err);
    if (hw.mux.available && root.exists(hw.mux.sysfsPath) && !writeSysfsValue(root, hw.mux.sysfsPath, "1", &err))
        say(err);
}

} // namespace

int main()
{
    const Root root = Root::fromEnvironment();
    const Mode configured = readConfiguredMode(root);
    say(QStringLiteral("configured mode: %1").arg(modeToString(configured)));

    if (configured != Mode::Integrated)
        wakeAsusDgpu(root);

    const Hardware hw = detectHardware(root);
    const Gpu *dgpu = hw.discreteGpu();

    Mode active = configured;
    QString failure;

    if (configured == Mode::Hybrid) {
        // always possible
        if (hw.mux.available && hw.mux.dedicatedActive) {
            active = Mode::MuxDedicated; // the firmware routes the panel to the dGPU regardless
            say(QStringLiteral("firmware MUX is in dedicated position"));
        }
    } else if (configured == Mode::Integrated && !dgpu) {
        // The udev rule already removed the GPU (or the machine has none we could see).
        say(QStringLiteral("no discrete GPU visible; assuming it was removed"));
    } else if (!supportedModes(hw).contains(configured)) {
        failure = QStringLiteral("mode %1 is not supported on this hardware (%2)")
                      .arg(modeToString(configured), hardwareClassToString(hw.hardwareClass));
    } else if (configured == Mode::MuxDedicated && !hw.mux.dedicatedActive) {
        failure = QStringLiteral("firmware MUX did not switch");
    } else if (configured == Mode::Integrated) {
        // Backstop for the udev rule (it may have lost the race with coldplug).
        QStringList errors;
        const QStringList removed = removeDiscreteGpu(root, *dgpu, &errors);
        for (const QString &a : removed)
            say(QStringLiteral("removed PCI device %1").arg(a));
        for (const QString &e : std::as_const(errors))
            say(e);
        QString err;
        if (root.exists(kAsusDgpuDisablePath) && !writeSysfsValue(root, kAsusDgpuDisablePath, "1", &err))
            say(err);
    }

    if (!failure.isEmpty()) {
        recordFailure(root, hw, failure);
        active = Mode::Hybrid;
    }

    QString err;
    if (!writeBootState(root, BootState{active, failure}, &err))
        say(err);
    else
        say(QStringLiteral("active mode: %1").arg(modeToString(active)));

    const QString offloadPath = QString::fromLatin1(kOffloadEnvPath);
    if (active == Mode::Hybrid && dgpu) {
        const QStringList env = offloadEnvironment(*dgpu, nvidiaGpuCount(hw));
        if (!root.writeFile(offloadPath, offloadEnvFileContents(env), 0644, &err))
            say(err);
        else
            say(QStringLiteral("wrote %1 (%2 variables)").arg(offloadPath).arg(env.size()));
    } else if (!root.removeFile(offloadPath, &err)) {
        say(err);
    }
    return 0;
}
