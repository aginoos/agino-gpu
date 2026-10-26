// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/mode.h"

#include "agino-gpu/configgen.h" // path constants only
#include "agino-gpu/detect.h"

#include <QFile>

namespace AginoGpu {

namespace {

QString path(const char *constant)
{
    return QString::fromLatin1(constant);
}

CheckResult makeResult(CheckResult::Level level, const char *code, const char *message)
{
    return {level, QString::fromLatin1(code), QString::fromLatin1(message)};
}

bool contains(const QList<Mode> &modes, Mode mode)
{
    return modes.contains(mode);
}

} // namespace

QString modeToString(Mode mode)
{
    switch (mode) {
    case Mode::Hybrid:
        return QStringLiteral("hybrid");
    case Mode::Integrated:
        return QStringLiteral("integrated");
    case Mode::Dedicated:
        return QStringLiteral("dedicated");
    case Mode::MuxDedicated:
        return QStringLiteral("mux-dedicated");
    }
    return QStringLiteral("hybrid");
}

std::optional<Mode> modeFromString(const QString &s)
{
    const QString v = s.trimmed();
    for (Mode m : {Mode::Hybrid, Mode::Integrated, Mode::Dedicated, Mode::MuxDedicated}) {
        if (v == modeToString(m))
            return m;
    }
    return std::nullopt;
}

QString pendingActionToString(PendingAction a)
{
    switch (a) {
    case PendingAction::None:
        return QStringLiteral("none");
    case PendingAction::Logout:
        return QStringLiteral("logout");
    case PendingAction::Reboot:
        return QStringLiteral("reboot");
    }
    return QStringLiteral("none");
}

QList<Mode> supportedModes(const Hardware &hw)
{
    switch (hw.hardwareClass) {
    case HardwareClass::Single:
        return {};
    case HardwareClass::Hybrid:
        return {Mode::Hybrid, Mode::Integrated, Mode::Dedicated};
    case HardwareClass::HybridMux:
        if (hw.mux.dedicatedActive)
            return {Mode::Hybrid, Mode::MuxDedicated};
        return {Mode::Hybrid, Mode::Integrated, Mode::Dedicated, Mode::MuxDedicated};
    case HardwareClass::Desktop: {
        const Gpu *dgpu = hw.discreteGpu();
        if (dgpu && !dgpu->connectedOutputs.isEmpty())
            return {Mode::Hybrid};
        return {Mode::Hybrid, Mode::Integrated};
    }
    case HardwareClass::Egpu:
        return {Mode::Hybrid};
    }
    return {};
}

QList<Mode> offeredModes(const Hardware &hw, Mode active)
{
    QList<Mode> modes = supportedModes(hw);
    if (modes.isEmpty() && !hw.discreteGpu() && active == Mode::Integrated)
        modes = {Mode::Hybrid, Mode::Integrated};
    return modes;
}

PendingAction pendingAction(Mode configured, Mode active)
{
    if (configured == active)
        return PendingAction::None;
    if (configured == Mode::MuxDedicated || active == Mode::MuxDedicated)
        return PendingAction::Reboot;
    if (configured == Mode::Integrated || active == Mode::Integrated)
        return PendingAction::Reboot;
    return PendingAction::Logout; // Dedicated <-> Hybrid
}

Mode readConfiguredMode(const Root &root)
{
    const auto text = root.readTrimmed(path(kModePath));
    if (!text)
        return Mode::Hybrid;
    return modeFromString(*text).value_or(Mode::Hybrid);
}

bool writeConfiguredMode(const Root &root, Mode mode, QString *error)
{
    return root.writeFile(path(kModePath), (modeToString(mode) + QLatin1Char('\n')).toUtf8(), 0644,
                          error);
}

std::optional<BootState> readBootState(const Root &root)
{
    const auto text = root.readTrimmed(path(kStatePath));
    if (!text)
        return std::nullopt;
    BootState state;
    const QStringList lines = text->split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        if (key == QLatin1String("active"))
            state.active = modeFromString(value).value_or(Mode::Hybrid);
        else if (key == QLatin1String("failure"))
            state.failure = value;
        else if (key == QLatin1String("attempted"))
            state.attempted = modeFromString(value).value_or(Mode::Hybrid);
    }
    return state;
}

bool writeBootState(const Root &root, const BootState &state, QString *error)
{
    // The value must stay on one line.
    QString failure = state.failure;
    failure.replace(QLatin1Char('\n'), QLatin1Char(' ')).replace(QLatin1Char('\r'), QLatin1Char(' '));
    const QString text = QStringLiteral("active=%1\nfailure=%2\nattempted=%3\n")
                             .arg(modeToString(state.active), failure, modeToString(state.attempted));
    return root.writeFile(path(kStatePath), text.toUtf8(), 0644, error);
}

QString readLastFailure(const Root &root)
{
    return root.readTrimmed(path(kLastFailurePath)).value_or(QString());
}

QList<CheckResult> checkMode(const Hardware &hw, Mode current, Mode target, const CheckContext &ctx)
{
    Q_UNUSED(current) // the checks depend on the hardware state, not on the mode being left
    using Level = CheckResult::Level;
    const Gpu *dgpu = hw.discreteGpu();
    const bool toIntegrated = target == Mode::Integrated;
    const bool toDedicated = target == Mode::Dedicated || target == Mode::MuxDedicated;

    // Specific refusals first (§3.3 table), so the UI gets the precise reason.
    if (hw.hardwareClass == HardwareClass::Egpu && (toIntegrated || target == Mode::Dedicated))
        return {makeResult(Level::Error, "egpu",
                           "An external GPU can be unplugged at any time; only hybrid mode is available.")};
    if (hw.hardwareClass == HardwareClass::Desktop && toIntegrated && dgpu
        && !dgpu->connectedOutputs.isEmpty())
        return {makeResult(Level::Error, "desktop-display-on-dgpu",
                           "A display is connected to the discrete GPU; integrated mode would leave it dark.")};
    if (toIntegrated && dgpu && hw.hasInternalPanel && hw.internalPanelGpu == dgpu->pciAddress)
        return {makeResult(Level::Error, "panel-on-dgpu",
                           "The built-in display is connected to the discrete GPU only; "
                           "switch the MUX back to hybrid first.")};
    if (!contains(supportedModes(hw), target))
        return {makeResult(Level::Error, "unsupported-mode",
                           "This mode is not available on this computer.")};

    QList<CheckResult> results;
    if (toIntegrated && dgpu) {
        if (!dgpu->connectedOutputs.isEmpty())
            results.append(makeResult(Level::Warning, "external-display-on-dgpu",
                                      "Displays connected to the discrete GPU will stop working."));
    }
    if (toDedicated) {
        if (ctx.secureBootEnabled && !ctx.nvidiaModuleSigned && dgpu && dgpu->vendor == Vendor::Nvidia)
            results.append(makeResult(Level::Warning, "secure-boot-unsigned",
                                      "Secure Boot is on and the NVIDIA driver is not signed; the "
                                      "screen may stay black."));
        if (ctx.onBattery)
            results.append(makeResult(Level::Info, "on-battery",
                                      "The discrete GPU stays on and drains the battery faster."));
    }
    return results;
}

CheckContext readCheckContext(const Root &root)
{
    CheckContext ctx;

    // Power: any Mains/USB supply online -> mains power. No supplies at all -> not on battery.
    const QStringList supplies = root.list(QStringLiteral("/sys/class/power_supply"));
    bool mainsOnline = false;
    for (const QString &name : supplies) {
        const QString base = QStringLiteral("/sys/class/power_supply/") + name;
        const QString type = root.readTrimmed(base + QStringLiteral("/type")).value_or(QString());
        const bool mains = type == QLatin1String("Mains") || type.startsWith(QLatin1String("USB"));
        if (mains && root.readTrimmed(base + QStringLiteral("/online")) == QLatin1String("1"))
            mainsOnline = true;
    }
    ctx.onBattery = !supplies.isEmpty() && !mainsOnline;

    // EFI variable: 4 attribute bytes, then the value (1 = enabled).
    QFile sb(root.path(QStringLiteral("/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c")));
    if (sb.open(QIODevice::ReadOnly)) {
        const QByteArray data = sb.readAll();
        ctx.secureBootEnabled = data.size() >= 5 && data.at(4) == 1;
    }

    // Taint flag "E": an unsigned module was loaded.
    if (const auto taint = root.readTrimmed(QStringLiteral("/sys/module/nvidia/taint")))
        ctx.nvidiaModuleSigned = !taint->contains(QLatin1Char('E'));
    return ctx;
}

} // namespace AginoGpu
