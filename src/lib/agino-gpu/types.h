// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace AginoGpu {

enum class Vendor { Unknown, Intel, Amd, Nvidia };

// design.md §2
enum class HardwareClass {
    Single,    // H0: one GPU
    Hybrid,    // H1: laptop, internal panel on the boot GPU
    HybridMux, // H2: H1 + a MUX interface (asus-nb-wmi gpu_mux_mode)
    Desktop,   // H3: no internal panel
    Egpu,      // H4: the discrete GPU sits behind Thunderbolt / is removable
};

// design.md §3.1. String forms: "hybrid", "integrated", "dedicated", "mux-dedicated".
enum class Mode { Hybrid, Integrated, Dedicated, MuxDedicated };

// design.md §3.2. String forms: "none", "logout", "reboot".
enum class PendingAction { None, Logout, Reboot };

struct Gpu
{
    QString pciAddress;     // "0000:01:00.0"
    Vendor vendor = Vendor::Unknown;
    quint16 vendorId = 0;   // 0x10de
    quint16 deviceId = 0;
    QString name;           // best effort, e.g. "NVIDIA GeForce RTX 5080 Laptop GPU"; may be empty
    QString driver;         // bound kernel driver ("i915", "xe", "amdgpu", "nvidia"); empty if none
    bool bootVga = false;
    bool discrete = false;  // not the boot VGA device, or an AMD/Intel card on its own PCIe slot
    bool removable = false; // behind Thunderbolt / USB4, or "removable" says so
    QString drmCard;        // "card1"; empty without a DRM driver
    QStringList connectedOutputs; // connector names without the card prefix: "eDP-1", "HDMI-A-1"
    QStringList allOutputs;       // every connector of this card
    bool runtimePmSupported = false; // power/control exists
    QString runtimeStatus;  // power/runtime_status: "active", "suspended", ... ; empty if unknown
    QStringList functions;  // other PCI functions of the same slot ("0000:01:00.1")
    QString upstreamPort;   // PCI address of the bridge above the device; empty for root bus devices
};

struct MuxInfo
{
    bool available = false;
    QString sysfsPath;      // absolute, without the Root prefix
    bool dedicatedActive = false; // the firmware currently routes the panel to the discrete GPU
};

struct Hardware
{
    QList<Gpu> gpus;
    HardwareClass hardwareClass = HardwareClass::Single;
    int integratedIndex = -1; // index into gpus of the boot / integrated GPU; -1 if none
    int discreteIndex = -1;   // the discrete GPU we manage (the first one); -1 if none
    bool hasInternalPanel = false;
    QString internalPanelGpu; // PCI address of the GPU driving eDP/LVDS/DSI
    MuxInfo mux;
    bool asusDgpuDisable = false; // /sys/devices/platform/asus-nb-wmi/dgpu_disable exists

    const Gpu *integrated() const { return integratedIndex >= 0 ? &gpus.at(integratedIndex) : nullptr; }
    const Gpu *discreteGpu() const { return discreteIndex >= 0 ? &gpus.at(discreteIndex) : nullptr; }
};

} // namespace AginoGpu
