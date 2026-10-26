// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QCoreApplication>
#include <QString>

namespace AginoGpu::QpTexts {

// Strings only the quick panel needs; everything shared is in agino-gpu-client/texts.h.
// Spelled out as QCoreApplication::translate calls so lupdate finds the context.

inline QString switchFailedTitle() { return QCoreApplication::translate("AginoGpuQuickPanel", "Graphics mode switch failed"); }
inline QString confirmSwitch() { return QCoreApplication::translate("AginoGpuQuickPanel", "Switch"); }
inline QString cancel() { return QCoreApplication::translate("AginoGpuQuickPanel", "Cancel"); }
inline QString dgpuAppsTitle() { return QCoreApplication::translate("AginoGpuQuickPanel", "Apps using the discrete GPU"); }
inline QString genericFailure() { return QCoreApplication::translate("AginoGpuQuickPanel", "Could not switch the graphics mode"); }

} // namespace AginoGpu::QpTexts
