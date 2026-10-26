// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QCoreApplication>
#include <QString>

namespace AginoGpu::QpTexts {

// Strings only the quick panel needs; everything shared is in agino-gpu-client/texts.h.
inline QString tr(const char *source)
{
    return QCoreApplication::translate("AginoGpuQuickPanel", source);
}

inline QString switchFailedTitle() { return tr("Graphics mode switch failed"); }
inline QString confirmSwitch() { return tr("Switch"); }
inline QString cancel() { return tr("Cancel"); }
inline QString dgpuAppsTitle() { return tr("Apps using the discrete GPU"); }
inline QString genericFailure() { return tr("Could not switch the graphics mode"); }

} // namespace AginoGpu::QpTexts
