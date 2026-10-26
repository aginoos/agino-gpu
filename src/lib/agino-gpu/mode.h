// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/root.h"
#include "agino-gpu/types.h"

#include <QList>
#include <QString>

#include <optional>

namespace AginoGpu {

QString modeToString(Mode mode);
std::optional<Mode> modeFromString(const QString &s);
QString pendingActionToString(PendingAction a);

// design.md §2 table / §3.3: which modes the UI may offer on this hardware. Single -> none.
QList<Mode> supportedModes(const Hardware &hw);
// What the daemon and the UI offer: supportedModes(), plus the hybrid/integrated pair when the
// discrete GPU is already gone because the active mode is integrated.
QList<Mode> offeredModes(const Hardware &hw, Mode active);

// design.md §3.2 rules 1–3: what the user must do for `configured` to take effect when
// `active` is running. Anything into or out of MuxDedicated is a reboot; Integrated is a
// reboot; Dedicated <-> Hybrid is a logout.
PendingAction pendingAction(Mode configured, Mode active);

// Persistent state files (design.md §6.3).
// /etc/agino-gpu/mode: one line, the mode string. Missing or invalid -> Hybrid.
Mode readConfiguredMode(const Root &root);
bool writeConfiguredMode(const Root &root, Mode mode, QString *error = nullptr);

// /run/agino-gpu/state: written by agino-gpu-apply at boot. "key=value" lines:
//   active=<mode>   failure=<text, may be empty>
struct BootState
{
    Mode active = Mode::Hybrid;
    QString failure;
};
std::optional<BootState> readBootState(const Root &root); // nullopt when the file is missing
bool writeBootState(const Root &root, const BootState &state, QString *error = nullptr);

// /var/lib/agino-gpu/last-failure: free text, kept until the user dismisses it.
QString readLastFailure(const Root &root);

// design.md §3.3 pre-switch checks.
struct CheckResult
{
    enum class Level { Info, Warning, Error }; // Error = refuse; Warning = needs force
    Level level = Level::Info;
    QString code;    // stable id for the UI: "external-display-on-dgpu", "panel-on-dgpu",
                     // "desktop-display-on-dgpu", "egpu", "secure-boot-unsigned", "on-battery",
                     // "unsupported-mode"
    QString message; // English, translated by the UI via the code
};

struct CheckContext
{
    bool onBattery = false;
    bool secureBootEnabled = false;
    bool nvidiaModuleSigned = true; // false when Secure Boot would refuse the nvidia module
};

QList<CheckResult> checkMode(const Hardware &hw, Mode current, Mode target, const CheckContext &ctx);

// Reads /sys/class/power_supply (any "Mains"/"USB" supply online -> not on battery) and the
// SecureBoot EFI variable; module signature state comes from /sys/module/nvidia/taint
// ("E" = unsigned module loaded) when loaded.
CheckContext readCheckContext(const Root &root);

} // namespace AginoGpu
