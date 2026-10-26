// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/root.h"
#include "agino-gpu/types.h"

namespace AginoGpu {

// Classifies the machine from sysfs only (design.md §2, §6.5): /sys/bus/pci/devices (class
// 0x03xxxx display controllers), /sys/class/drm, /sys/devices/platform/asus-nb-wmi. Must never
// wake a suspended GPU: no config space reads, no device nodes, no tools. Connector "status"
// is safe: the kernel returns the cached value (drm_sysfs.c status_show, READ_ONCE; V-12).
Hardware detectHardware(const Root &root);

// Re-reads only the volatile fields (runtimeStatus, connectedOutputs) of an already detected
// GPU; cheap enough for the 2 s poll of the quick panel.
void refreshGpu(const Root &root, Gpu &gpu);

QString vendorName(Vendor vendor);
QString hardwareClassToString(HardwareClass c); // "single", "hybrid", "hybrid-mux", "desktop", "egpu"

} // namespace AginoGpu
