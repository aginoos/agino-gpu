// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu-client/texts.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

namespace AginoGpu::Texts {

// Every string is spelled out in a QCoreApplication::translate call so lupdate finds it.

void installTranslator()
{
    static bool installed = false;
    if (installed || !QCoreApplication::instance())
        return;
    installed = true;
    auto *translator = new QTranslator(QCoreApplication::instance());
    if (translator->load(QLocale(), QStringLiteral("agino-gpu"), QStringLiteral("_"),
                         QStringLiteral(AGINO_GPU_TRANSLATIONS_DIR)))
        QCoreApplication::installTranslator(translator);
    else
        delete translator;
}

QString title()
{
    return QCoreApplication::translate("AginoGpu", "Graphics");
}

QString modeName(Mode mode)
{
    switch (mode) {
    case Mode::Hybrid:
        return QCoreApplication::translate("AginoGpu", "Hybrid");
    case Mode::Integrated:
        return QCoreApplication::translate("AginoGpu", "Integrated");
    case Mode::Dedicated:
        return QCoreApplication::translate("AginoGpu", "Dedicated");
    case Mode::MuxDedicated:
        return QCoreApplication::translate("AginoGpu", "Direct");
    }
    return QString();
}

QString modeSummary(Mode mode)
{
    switch (mode) {
    case Mode::Hybrid:
        return QCoreApplication::translate("AginoGpu", "Uses the discrete GPU when needed");
    case Mode::Integrated:
        return QCoreApplication::translate("AginoGpu", "Longest battery life");
    case Mode::Dedicated:
        return QCoreApplication::translate("AginoGpu", "Everything renders on the discrete GPU");
    case Mode::MuxDedicated:
        return QCoreApplication::translate("AginoGpu", "Best performance");
    }
    return QString();
}

QString pendingCost(PendingAction action)
{
    switch (action) {
    case PendingAction::Reboot:
        return QCoreApplication::translate("AginoGpu", "Restart required");
    case PendingAction::Logout:
        return QCoreApplication::translate("AginoGpu", "Log out required");
    case PendingAction::None:
        break;
    }
    return QString();
}

QString pendingStatus(PendingAction action)
{
    switch (action) {
    case PendingAction::Reboot:
        return QCoreApplication::translate("AginoGpu", "Restart pending");
    case PendingAction::Logout:
        return QCoreApplication::translate("AginoGpu", "Log out pending");
    case PendingAction::None:
        break;
    }
    return QString();
}

QString switchFailed()
{
    return QCoreApplication::translate("AginoGpu", "Switch failed");
}

QString dgpuStatus(DgpuStatus status)
{
    switch (status) {
    case DgpuStatus::Suspended:
        return QCoreApplication::translate("AginoGpu", "Sleeping");
    case DgpuStatus::Active:
        return QCoreApplication::translate("AginoGpu", "Active");
    case DgpuStatus::Off:
        return QCoreApplication::translate("AginoGpu", "Off");
    case DgpuStatus::None:
        break;
    }
    return QString();
}

QString pendingBanner(Mode configured, PendingAction action)
{
    if (action == PendingAction::Reboot)
        return QCoreApplication::translate("AginoGpu", "Switches to %1 after restart").arg(modeName(configured));
    if (action == PendingAction::Logout)
        return QCoreApplication::translate("AginoGpu", "Switches to %1 after you log out").arg(modeName(configured));
    return QString();
}

QString failureBanner(std::optional<Mode> failed)
{
    if (!failed)
        return QCoreApplication::translate("AginoGpu", "Could not switch the graphics mode; back to %1")
            .arg(modeName(Mode::Hybrid));
    return QCoreApplication::translate("AginoGpu", "Could not switch to %1; back to %2")
        .arg(modeName(*failed), modeName(Mode::Hybrid));
}

QString checkText(const CheckItem &item, const QStringList &outputs)
{
    const QString &code = item.code;
    if (code == QLatin1String("external-display-on-dgpu")) {
        const QString names = outputs.join(QLatin1String(", "));
        return names.isEmpty() ? QCoreApplication::translate("AginoGpu", "Displays on the discrete GPU will not work in Integrated mode")
                               : QCoreApplication::translate("AginoGpu", "%1 is connected to the discrete GPU and will not work in Integrated mode")
                                     .arg(names);
    }
    if (code == QLatin1String("panel-on-dgpu"))
        return QCoreApplication::translate("AginoGpu", "The built-in display runs on the discrete GPU. Switch the MUX back first.");
    if (code == QLatin1String("desktop-display-on-dgpu"))
        return QCoreApplication::translate("AginoGpu", "A display is connected to the discrete GPU");
    if (code == QLatin1String("egpu"))
        return QCoreApplication::translate("AginoGpu", "External GPUs only support Hybrid mode");
    if (code == QLatin1String("secure-boot-unsigned"))
        return QCoreApplication::translate("AginoGpu", "Secure Boot is on and the NVIDIA driver is not signed. The screen may stay black.");
    if (code == QLatin1String("on-battery"))
        return QCoreApplication::translate("AginoGpu", "The discrete GPU stays on and uses more battery");
    if (code == QLatin1String("unsupported-mode"))
        return QCoreApplication::translate("AginoGpu", "Not available on this computer");
    return item.message;
}

QString undo()
{
    return QCoreApplication::translate("AginoGpu", "Undo");
}

QString restartNow()
{
    return QCoreApplication::translate("AginoGpu", "Restart Now");
}

QString logoutNow()
{
    return QCoreApplication::translate("AginoGpu", "Log Out Now");
}

QString details()
{
    return QCoreApplication::translate("AginoGpu", "Details");
}

QString runOnDgpu()
{
    return QCoreApplication::translate("AginoGpu", "Run with Discrete Graphics");
}

QString settings()
{
    return QCoreApplication::translate("AginoGpu", "Graphics Settings");
}

} // namespace AginoGpu::Texts
