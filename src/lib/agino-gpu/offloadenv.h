// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/root.h"
#include "agino-gpu/types.h"

#include <QStringList>

namespace AginoGpu {

// design.md §5.1: the variables that make an application render on `dgpu`, as "KEY=VALUE".
// The single source of truth for the DAM rules, the right-click menus and agino-gpu-run.
//   NVIDIA: __NV_PRIME_RENDER_OFFLOAD=1, (__NV_PRIME_RENDER_OFFLOAD_PROVIDER=NVIDIA-G<n> only
//           when nvidiaCount > 1), __GLX_VENDOR_LIBRARY_NAME=nvidia,
//           __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/10_nvidia.json,
//           __VK_LAYER_NV_optimus=NVIDIA_only
//   AMD/Intel (Mesa): DRI_PRIME=pci-0000_03_00_0, MESA_VK_DEVICE_SELECT=<vendor>:<device>
//           (lower-case hex, no 0x), plus DXVK_FILTER_DEVICE_NAME / VKD3D_FILTER_DEVICE_NAME
//           when vulkanDeviceName is not empty.
// Order is fixed (tests compare byte for byte).
QStringList offloadEnvironment(const Gpu &dgpu, int nvidiaCount = 1,
                               const QString &vulkanDeviceName = QString());

// /run/agino-gpu/offload.env (design.md §4.1, §6.3): header comment line, then one KEY=VALUE
// per line. The file exists only while the active mode is hybrid and a discrete GPU is present.
QByteArray offloadEnvFileContents(const QStringList &environment);
QStringList parseOffloadEnvFile(const QByteArray &contents); // ignores comments/blank lines
QStringList readOffloadEnv(const Root &root);                // empty when the file is missing

} // namespace AginoGpu
