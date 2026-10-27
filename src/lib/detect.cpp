// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/detect.h"

#include <QHash>
#include <QRegularExpression>

#include <utility>

namespace AginoGpu {

namespace {

constexpr quint16 kVendorIntel = 0x8086;
constexpr quint16 kVendorAmd = 0x1002;
constexpr quint16 kVendorNvidia = 0x10de;

constexpr char kMuxPath[] = "/sys/devices/platform/asus-nb-wmi/gpu_mux_mode";
constexpr char kDgpuDisablePath[] = "/sys/devices/platform/asus-nb-wmi/dgpu_disable";

QString pciDevicePath(const QString &address)
{
    return QStringLiteral("/sys/bus/pci/devices/") + address;
}

// Qt resolves a relative link target lexically, so a link read through another symbolic link
// ("/sys/bus/pci/devices/<a>/driver", "/sys/class/drm/card1/device") would be resolved
// against the wrong directory. Always read links from the physical directory instead.
QString physicalPath(const Root &root, const QString &path)
{
    const QString target = root.linkTarget(path);
    return target.isEmpty() ? path : target;
}

QString baseName(const QString &path)
{
    return path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
}

QString parentPath(const QString &path)
{
    return path.left(path.lastIndexOf(QLatin1Char('/')));
}

quint16 parseHex(const QString &text)
{
    QString s = text;
    if (s.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        s = s.mid(2);
    bool ok = false;
    const uint v = s.toUInt(&ok, 16);
    return ok && v <= 0xffff ? quint16(v) : quint16(0);
}

Vendor vendorFromId(quint16 id)
{
    switch (id) {
    case kVendorIntel:
        return Vendor::Intel;
    case kVendorAmd:
        return Vendor::Amd;
    case kVendorNvidia:
        return Vendor::Nvidia;
    default:
        return Vendor::Unknown;
    }
}

bool isPanelConnector(const QString &name)
{
    return name.startsWith(QLatin1String("eDP")) || name.startsWith(QLatin1String("LVDS"))
        || name.startsWith(QLatin1String("DSI"));
}

struct DrmInfo
{
    QString card;
    QStringList all;
    QStringList connected;
};

// Connectors of one card: /sys/class/drm/<card>-<NAME>; "status" is the cached value.
void readConnectors(const Root &root, const QString &card, QStringList *all, QStringList *connected)
{
    all->clear();
    connected->clear();
    const QString prefix = card + QLatin1Char('-');
    const QString base = QStringLiteral("/sys/class/drm/");
    const QStringList entries = root.list(QStringLiteral("/sys/class/drm"));
    for (const QString &entry : entries) {
        if (!entry.startsWith(prefix))
            continue;
        const QString name = entry.mid(prefix.size());
        all->append(name);
        if (root.readTrimmed(base + entry + QStringLiteral("/status")) == QLatin1String("connected"))
            connected->append(name);
    }
}

// PCI address -> DRM card, for every /sys/class/drm/cardN (not the connectors, not renderD).
QHash<QString, DrmInfo> scanDrm(const Root &root)
{
    QHash<QString, DrmInfo> result;
    static const QRegularExpression cardRe(QStringLiteral("^card[0-9]+$"));
    const QStringList entries = root.list(QStringLiteral("/sys/class/drm"));
    for (const QString &entry : entries) {
        if (!cardRe.match(entry).hasMatch())
            continue;
        const QString physical = physicalPath(root, QStringLiteral("/sys/class/drm/") + entry);
        const QString device = root.linkTarget(physical + QStringLiteral("/device"));
        if (device.isEmpty())
            continue;
        const QString address = baseName(device);
        if (result.contains(address))
            continue;
        DrmInfo info;
        info.card = entry;
        readConnectors(root, entry, &info.all, &info.connected);
        result.insert(address, info);
    }
    return result;
}

// Thunderbolt / USB4 detection, kept simple: the device itself or any PCI bridge above it
// reports removable == "removable", or a path component is "thunderbolt" or "domainN" (the
// Thunderbolt domain nodes in the device tree).
bool isRemovable(const Root &root, const QString &devicePath)
{
    static const QRegularExpression domainRe(QStringLiteral("^domain[0-9]+$"));
    const QStringList parts = devicePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QLatin1String("thunderbolt") || domainRe.match(part).hasMatch())
            return true;
    }
    QString path = devicePath;
    while (root.exists(path + QStringLiteral("/vendor"))) {
        if (root.readTrimmed(path + QStringLiteral("/removable")) == QLatin1String("removable"))
            return true;
        path = parentPath(path);
    }
    return false;
}

QString lookupPciName(const Root &root, quint16 vendorId, quint16 deviceId, Vendor vendor)
{
    std::optional<QString> text;
    for (const char *file : {"/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids"}) {
        text = root.readTrimmed(QString::fromLatin1(file));
        if (text)
            break;
    }
    if (!text || vendorId == 0)
        return QString();

    const QString vendorKey = QStringLiteral("%1  ").arg(vendorId, 4, 16, QLatin1Char('0'));
    const QString deviceKey = QStringLiteral("%1  ").arg(deviceId, 4, 16, QLatin1Char('0'));
    QString vendorText;
    QString deviceText;
    bool inVendor = false;
    const QStringList lines = text->split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('\t'))) {
            if (inVendor && !line.startsWith(QLatin1String("\t\t"))
                && line.mid(1).startsWith(deviceKey)) {
                deviceText = line.mid(1 + deviceKey.size()).trimmed();
                break;
            }
            continue;
        }
        if (inVendor)
            break; // next vendor: not found
        if (line.startsWith(vendorKey)) {
            inVendor = true;
            vendorText = line.mid(vendorKey.size()).trimmed();
        }
    }
    if (deviceText.isEmpty())
        return QString();

    QString model = deviceText;
    const int open = deviceText.indexOf(QLatin1Char('['));
    const int close = deviceText.lastIndexOf(QLatin1Char(']'));
    if (open >= 0 && close > open)
        model = deviceText.mid(open + 1, close - open - 1).trimmed();

    QString prefix;
    switch (vendor) {
    case Vendor::Nvidia:
        prefix = QStringLiteral("NVIDIA");
        break;
    case Vendor::Intel:
        prefix = QStringLiteral("Intel");
        break;
    case Vendor::Amd:
        prefix = QStringLiteral("AMD");
        break;
    case Vendor::Unknown:
        prefix = vendorText.section(QLatin1Char(' '), 0, 0);
        break;
    }
    if (prefix.isEmpty() || model.startsWith(prefix, Qt::CaseInsensitive))
        return model;
    return prefix + QLatin1Char(' ') + model;
}

bool readGpu(const Root &root, const QString &address, const QHash<QString, DrmInfo> &drm, Gpu *gpu)
{
    const QString link = pciDevicePath(address);
    const QString devicePath = physicalPath(root, link);
    const auto cls = root.readTrimmed(devicePath + QStringLiteral("/class"));
    if (!cls || !cls->startsWith(QLatin1String("0x03")))
        return false;

    Gpu g;
    g.pciAddress = address;
    g.vendorId = parseHex(root.readTrimmed(devicePath + QStringLiteral("/vendor")).value_or(QString()));
    g.deviceId = parseHex(root.readTrimmed(devicePath + QStringLiteral("/device")).value_or(QString()));
    g.vendor = vendorFromId(g.vendorId);
    g.driver = baseName(root.linkTarget(devicePath + QStringLiteral("/driver")));
    g.bootVga = root.readTrimmed(devicePath + QStringLiteral("/boot_vga")) == QLatin1String("1");
    g.runtimePmSupported = root.exists(devicePath + QStringLiteral("/power/control"));
    g.name = lookupPciName(root, g.vendorId, g.deviceId, g.vendor);

    const QString parent = parentPath(devicePath);
    if (root.exists(parent + QStringLiteral("/vendor")))
        g.upstreamPort = baseName(parent);
    g.removable = isRemovable(root, devicePath);

    // Other functions of the same slot: same "domain:bus:slot".
    const QString slot = address.left(address.lastIndexOf(QLatin1Char('.')));
    const QStringList all = root.list(QStringLiteral("/sys/bus/pci/devices"));
    for (const QString &other : all) {
        if (other != address && other.startsWith(slot + QLatin1Char('.')))
            g.functions.append(other);
    }

    const auto it = drm.constFind(address);
    if (it != drm.constEnd()) {
        g.drmCard = it->card;
        g.allOutputs = it->all;
        g.connectedOutputs = it->connected;
    }
    g.runtimeStatus =
        root.readTrimmed(devicePath + QStringLiteral("/power/runtime_status")).value_or(QString());
    *gpu = g;
    return true;
}

} // namespace

bool displayWithoutDiscreteGpu(const Hardware &hw)
{
    for (const Gpu &g : hw.gpus) {
        if (!g.discrete && !g.connectedOutputs.isEmpty())
            return true;
    }
    return false;
}

QString vendorName(Vendor vendor)
{
    switch (vendor) {
    case Vendor::Intel:
        return QStringLiteral("Intel");
    case Vendor::Amd:
        return QStringLiteral("AMD");
    case Vendor::Nvidia:
        return QStringLiteral("NVIDIA");
    case Vendor::Unknown:
        break;
    }
    return QStringLiteral("Unknown");
}

QString hardwareClassToString(HardwareClass c)
{
    switch (c) {
    case HardwareClass::Single:
        return QStringLiteral("single");
    case HardwareClass::Hybrid:
        return QStringLiteral("hybrid");
    case HardwareClass::HybridMux:
        return QStringLiteral("hybrid-mux");
    case HardwareClass::Desktop:
        return QStringLiteral("desktop");
    case HardwareClass::Egpu:
        return QStringLiteral("egpu");
    }
    return QStringLiteral("single");
}

void refreshGpu(const Root &root, Gpu &gpu)
{
    gpu.runtimeStatus = root.readTrimmed(pciDevicePath(gpu.pciAddress)
                                         + QStringLiteral("/power/runtime_status"))
                            .value_or(QString());
    QStringList all;
    QStringList connected;
    if (!gpu.drmCard.isEmpty())
        readConnectors(root, gpu.drmCard, &all, &connected);
    gpu.connectedOutputs = connected;
}

Hardware detectHardware(const Root &root)
{
    Hardware hw;

    const QHash<QString, DrmInfo> drm = scanDrm(root);
    const QStringList addresses = root.list(QStringLiteral("/sys/bus/pci/devices"));
    for (const QString &address : addresses) {
        Gpu gpu;
        if (readGpu(root, address, drm, &gpu))
            hw.gpus.append(gpu);
    }

    hw.mux.sysfsPath = QString::fromLatin1(kMuxPath);
    if (const auto mux = root.readTrimmed(hw.mux.sysfsPath)) {
        hw.mux.available = true;
        hw.mux.dedicatedActive = *mux == QLatin1String("0");
    } else if (root.exists(hw.mux.sysfsPath)) {
        hw.mux.available = true; // present but unreadable
    } else {
        hw.mux.sysfsPath.clear();
    }
    hw.asusDgpuDisable = root.exists(QString::fromLatin1(kDgpuDisablePath));

    // Integrated = the boot VGA device, fallback an Intel GPU on bus 00. Everything else is
    // discrete. With a single GPU nothing is discrete, except while the MUX routes the panel
    // to the discrete GPU (it is then the only visible one).
    if (hw.gpus.size() >= 2) {
        for (int i = 0; i < hw.gpus.size() && hw.integratedIndex < 0; ++i) {
            if (hw.gpus.at(i).bootVga)
                hw.integratedIndex = i;
        }
        for (int i = 0; i < hw.gpus.size() && hw.integratedIndex < 0; ++i) {
            const Gpu &g = hw.gpus.at(i);
            if (g.vendor == Vendor::Intel && g.pciAddress.mid(5, 2) == QLatin1String("00"))
                hw.integratedIndex = i;
        }
        for (int i = 0; i < hw.gpus.size(); ++i)
            hw.gpus[i].discrete = i != hw.integratedIndex;
    } else if (hw.gpus.size() == 1 && hw.mux.available && hw.mux.dedicatedActive) {
        hw.gpus[0].discrete = true;
    }
    // The managed discrete GPU: the first fixed one, else the first discrete one.
    for (int i = 0; i < hw.gpus.size() && hw.discreteIndex < 0; ++i) {
        if (hw.gpus.at(i).discrete && !hw.gpus.at(i).removable)
            hw.discreteIndex = i;
    }
    for (int i = 0; i < hw.gpus.size() && hw.discreteIndex < 0; ++i) {
        if (hw.gpus.at(i).discrete)
            hw.discreteIndex = i;
    }

    // Internal panel: eDP / LVDS / DSI connector that is connected, or any such connector on
    // the boot GPU.
    for (const Gpu &g : std::as_const(hw.gpus)) {
        for (const QString &name : g.allOutputs) {
            if (!isPanelConnector(name))
                continue;
            if (g.connectedOutputs.contains(name) || g.bootVga) {
                hw.hasInternalPanel = true;
                hw.internalPanelGpu = g.pciAddress;
                break;
            }
        }
        if (hw.hasInternalPanel)
            break;
    }

    // Classification, design.md §2.
    const Gpu *dgpu = hw.discreteGpu();
    const bool muxDedicated = hw.mux.available && hw.mux.dedicatedActive;
    if (hw.gpus.size() < 2 && !muxDedicated)
        hw.hardwareClass = HardwareClass::Single;
    else if (dgpu && dgpu->removable)
        hw.hardwareClass = HardwareClass::Egpu;
    else if (hw.mux.available && (hw.hasInternalPanel || muxDedicated))
        hw.hardwareClass = HardwareClass::HybridMux;
    else if (!hw.hasInternalPanel)
        hw.hardwareClass = HardwareClass::Desktop;
    else
        hw.hardwareClass = HardwareClass::Hybrid;
    return hw;
}

} // namespace AginoGpu
