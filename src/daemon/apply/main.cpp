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

// ASUS laptops create asus-nb-wmi (gpu_mux_mode, dgpu_disable) when the platform module
// loads, which can be shortly after udev coldplug. Wait a little so the MUX is not mistaken for
// missing.
void waitForAsusPlatform(const Root &root)
{
    const auto vendor = root.readTrimmed(QStringLiteral("/sys/class/dmi/id/sys_vendor"));
    if (!vendor || !vendor->startsWith(QLatin1String("ASUS")))
        return;
    for (int i = 0; i < 50 && !root.exists(QStringLiteral("/sys/devices/platform/asus-nb-wmi")); ++i)
        QThread::msleep(100);
}

// After the discrete GPU is gone, the integrated GPU must have a screen. Its DRM driver may
// still be probing, so poll the connectors for up to 10 s.
bool integratedHasDisplay(const Root &root)
{
    for (int i = 0; i < 50; ++i) {
        const Hardware now = detectHardware(root);
        const Gpu *igpu = now.integrated();
        if (igpu && !igpu->connectedOutputs.isEmpty())
            return true;
        QThread::msleep(200);
    }
    return false;
}

// Undo integrated mode: keep the udev helper away, power the GPU on, rescan for a removed GPU
// and replay "add" for a visible one, so udev loads its driver now that the modprobe block is
// gone (recordFailure with persist removes it first).
void restoreDiscreteGpu(const Root &root, const Hardware &hw)
{
    QString err;
    if (!root.writeFile(kKeepDgpuPath, QByteArray(), 0644, &err))
        say(err);
    if (root.readTrimmed(kAsusDgpuDisablePath).value_or(QString()) == QLatin1String("1")
        && !writeSysfsValue(root, kAsusDgpuDisablePath, "0", &err))
        say(err);
    if (!writeSysfsValue(root, kPciRescanPath, "1", &err))
        say(err);
    else
        say(QStringLiteral("discrete GPU restored (PCI rescan)"));
    if (const Gpu *dgpu = hw.discreteGpu()) {
        for (const QString &addr : QStringList{dgpu->pciAddress} + dgpu->functions) {
            if (!writeSysfsValue(root, QStringLiteral("/sys/bus/pci/devices/%1/uevent").arg(addr), "add", &err))
                say(err);
        }
    }
}

// persist=false: the saved mode stays as the user chose it and is tried again next boot (the
// cause may be transient: a device that showed up late, a monitor that was off).
void recordFailure(const Root &root, const Hardware &hw, const QString &reason, bool persist)
{
    say(QStringLiteral("FAILURE: %1; using hybrid for this boot%2")
            .arg(reason, persist ? QStringLiteral(" and from now on") : QString()));
    QString err;
    if (!root.writeFile(QString::fromLatin1(kLastFailurePath), (reason + QLatin1Char('\n')).toUtf8(), 0644, &err))
        say(err);
    if (!persist)
        return;
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

    waitForAsusPlatform(root);
    if (configured != Mode::Integrated)
        wakeAsusDgpu(root);

    Hardware hw = detectHardware(root);
    Mode active = configured;
    QString failure;
    bool persist = false;
    bool restoreDgpu = false;

    if (configured == Mode::Hybrid) {
        if (hw.mux.available && hw.mux.dedicatedActive) {
            active = Mode::MuxDedicated; // the firmware routes the panel to the dGPU regardless
            say(QStringLiteral("firmware MUX is in dedicated position"));
        }
    } else if (configured == Mode::Integrated) {
        if (const Gpu *dgpu = hw.discreteGpu()) {
            // Still visible: the udev helper declined (or lost the race). Remove it here only
            // when this hardware supports integrated mode.
            if (!supportedModes(hw).contains(Mode::Integrated)) {
                failure = QStringLiteral("integrated mode is not possible on this hardware (%1)")
                              .arg(hardwareClassToString(hw.hardwareClass));
                persist = true; // the modprobe block must go, or the visible GPU stays driverless
                restoreDgpu = true;
            } else {
                QStringList errors;
                for (const QString &a : removeDiscreteGpu(root, *dgpu, &errors))
                    say(QStringLiteral("removed PCI device %1").arg(a));
                for (const QString &e : std::as_const(errors))
                    say(e);
            }
        } else {
            say(QStringLiteral("no discrete GPU visible; it was removed at coldplug"));
        }
        if (failure.isEmpty() && !integratedHasDisplay(root)) {
            // Persisted: the modprobe block must go before the GPU comes back, or the restored
            // GPU gets no driver and a screen on it stays dark.
            failure = QStringLiteral("the integrated GPU has no connected display");
            persist = true;
            restoreDgpu = true;
        } else if (failure.isEmpty()) {
            QString err;
            if (root.exists(kAsusDgpuDisablePath) && !writeSysfsValue(root, kAsusDgpuDisablePath, "1", &err))
                say(err);
        }
    } else if (!hw.discreteGpu()) {
        failure = QStringLiteral("no discrete GPU visible for mode %1").arg(modeToString(configured));
    } else if (configured == Mode::MuxDedicated && !hw.mux.available) {
        failure = QStringLiteral("the firmware MUX interface is missing");
    } else if (!supportedModes(hw).contains(configured)) {
        failure = QStringLiteral("mode %1 is not supported on this hardware (%2)")
                      .arg(modeToString(configured), hardwareClassToString(hw.hardwareClass));
        persist = true;
    } else if (configured == Mode::MuxDedicated && !hw.mux.dedicatedActive) {
        failure = QStringLiteral("firmware MUX did not switch");
        persist = true;
    }

    if (!failure.isEmpty()) {
        recordFailure(root, hw, failure, persist);
        // The MUX position is latched for this boot; recordFailure only moves it for the next.
        active = hw.mux.available && hw.mux.dedicatedActive ? Mode::MuxDedicated : Mode::Hybrid;
    }
    if (restoreDgpu) {
        restoreDiscreteGpu(root, hw);
        hw = detectHardware(root);
    }

    QString err;
    if (!writeBootState(root, BootState{active, failure, configured}, &err))
        say(err);
    else
        say(QStringLiteral("active mode: %1").arg(modeToString(active)));

    const Gpu *dgpu = hw.discreteGpu();
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
