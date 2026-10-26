// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/root.h"
#include "agino-gpu/types.h"

namespace AginoGpu {

// Classifies the machine from sysfs only (design.md §2, §6.5): /sys/bus/pci/devices (class
// 0x03xxxx display controllers), /sys/class/drm, /sys/devices/platform/asus-nb-wmi. Must never
// wake a suspended GPU: no config space reads, no DRM connector "status" reads of a card whose
// device is runtime-suspended (use "enabled"/"status" only when runtime_status is "active", or
// fall back to the cached "dpms"/"enabled" attributes — see §9.4 V-12).
Hardware detectHardware(const Root &root);

// Re-reads only the volatile fields (runtimeStatus, connectedOutputs) of an already detected
// GPU; cheap enough for the 2 s poll of the quick panel.
void refreshGpu(const Root &root, Gpu &gpu);

QString vendorName(Vendor vendor);
QString hardwareClassToString(HardwareClass c); // "single", "hybrid", "hybrid-mux", "desktop", "egpu"

} // namespace AginoGpu
