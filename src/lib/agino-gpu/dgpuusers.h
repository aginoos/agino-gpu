// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/root.h"
#include "agino-gpu/types.h"

#include <QList>
#include <QString>

#include <optional>

namespace AginoGpu {

struct GpuUser
{
    quint32 pid = 0;
    quint32 uid = 0;
    QString name; // /proc/<pid>/comm
};

// Processes with the discrete GPU open (design.md §6.2 GetDgpuUsers, §6.5): any fd whose link
// target is /dev/nvidia[0-9]*, /dev/nvidiactl, /dev/nvidia-uvm*, /dev/nvidia-modeset, or the
// /dev/dri/cardN or /dev/dri/renderDN that belongs to `gpu` (matched through
// /sys/class/drm/<node>/device -> the GPU's PCI address). Reads /proc only; never opens a
// device. With onlyUid set, other users' processes are skipped (and their fd directories are
// not even read). Sorted by pid.
QList<GpuUser> findGpuUsers(const Root &root, const Gpu &gpu, std::optional<quint32> onlyUid = std::nullopt);

} // namespace AginoGpu
