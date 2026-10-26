// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu-client/gpucontrol.h"
#include "agino-gpu-client/gpustate.h"

#include <QString>

#include <optional>

namespace AginoGpu {

// UI strings shared by both plugins (design.md §7.3), translated in context "AginoGpu" from
// /usr/share/agino-gpu/translations/agino-gpu_<locale>.qm. Call installTranslator() once per
// process before using them.
namespace Texts {

void installTranslator();

QString title();                          // "Graphics" / 显卡
QString modeName(Mode mode);              // Hybrid / Integrated / Dedicated / Direct
QString modeSummary(Mode mode);           // one short line, control-center mode cards
QString pendingCost(PendingAction action); // "Restart required" / "Log out required"; empty for None
QString pendingStatus(PendingAction action); // tile subtitle: "Restart pending" / "Log out pending"
QString switchFailed();                   // tile subtitle after a failed boot
QString dgpuStatus(DgpuStatus status);    // Sleeping / Active / Off
// Banner after SetMode succeeded and something is still to do: "Switches to Integrated after
// restart".
QString pendingBanner(Mode configured, PendingAction action);
// "Could not switch to Dedicated; back to Hybrid"; without the mode (failure from an earlier
// boot): "Could not switch the graphics mode; back to Hybrid".
QString failureBanner(std::optional<Mode> failed);
// CheckMode result; `outputs` names the connectors on the discrete GPU for the
// external-display warning.
QString checkText(const CheckItem &item, const QStringList &outputs);

QString undo();        // 撤销
QString restartNow();  // 立即重启
QString logoutNow();   // 立即注销
QString details();     // 详情
QString runOnDgpu();   // "Run with discrete graphics" (fixed menu text, design.md §7.3)
QString settings();    // "Graphics Settings" (quick panel link to the control center)

} // namespace Texts

} // namespace AginoGpu
