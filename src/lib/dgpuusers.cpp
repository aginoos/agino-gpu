// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/dgpuusers.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace AginoGpu {

namespace {

// DRM nodes (card0, renderD128) whose device is the given PCI address, as /dev/dri paths.
QSet<QString> drmNodesOf(const Root &root, const QString &pciAddress)
{
    QSet<QString> nodes;
    static const QRegularExpression nodeRe(QStringLiteral("^(card[0-9]+|renderD[0-9]+)$"));
    const QStringList entries = root.list(QStringLiteral("/sys/class/drm"));
    for (const QString &entry : entries) {
        if (!nodeRe.match(entry).hasMatch())
            continue;
        // Read the "device" link from the physical directory (see detect.cpp).
        QString physical = root.linkTarget(QStringLiteral("/sys/class/drm/") + entry);
        if (physical.isEmpty())
            physical = QStringLiteral("/sys/class/drm/") + entry;
        const QString device = root.linkTarget(physical + QStringLiteral("/device"));
        if (device.mid(device.lastIndexOf(QLatin1Char('/')) + 1) == pciAddress)
            nodes.insert(QStringLiteral("/dev/dri/") + entry);
    }
    return nodes;
}

bool isNvidiaNode(const QString &target)
{
    static const QRegularExpression re(
        QStringLiteral("^/dev/(nvidia[0-9]+|nvidiactl|nvidia-uvm.*|nvidia-modeset)$"));
    return re.match(target).hasMatch();
}

std::optional<quint32> readUid(const Root &root, const QString &procDir)
{
    const auto status = root.readTrimmed(procDir + QStringLiteral("/status"));
    if (!status)
        return std::nullopt;
    const QStringList lines = status->split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (!line.startsWith(QLatin1String("Uid:")))
            continue;
        const QStringList fields = line.mid(4).simplified().split(QLatin1Char(' '));
        bool ok = false;
        const quint32 uid = fields.value(0).toUInt(&ok);
        return ok ? std::optional<quint32>(uid) : std::nullopt;
    }
    return std::nullopt;
}

} // namespace

QList<GpuUser> findGpuUsers(const Root &root, const Gpu &gpu, std::optional<quint32> onlyUid)
{
    const QSet<QString> drmNodes = drmNodesOf(root, gpu.pciAddress);
    const bool nvidia = gpu.vendor == Vendor::Nvidia;

    QList<GpuUser> users;
    const QStringList entries = root.list(QStringLiteral("/proc"));
    for (const QString &entry : entries) {
        bool isPid = false;
        const quint32 pid = entry.toUInt(&isPid);
        if (!isPid)
            continue;
        const QString procDir = QStringLiteral("/proc/") + entry;
        const auto uid = readUid(root, procDir);
        if (!uid || (onlyUid && *uid != *onlyUid))
            continue;

        bool uses = false;
        const QStringList fds = root.list(procDir + QStringLiteral("/fd"));
        for (const QString &fd : fds) {
            const QString target = root.linkTarget(procDir + QStringLiteral("/fd/") + fd);
            if (target.isEmpty())
                continue;
            if (drmNodes.contains(target) || (nvidia && isNvidiaNode(target))) {
                uses = true;
                break;
            }
        }
        if (!uses)
            continue;
        GpuUser user;
        user.pid = pid;
        user.uid = *uid;
        user.name = root.readTrimmed(procDir + QStringLiteral("/comm")).value_or(QString());
        users.append(user);
    }
    std::sort(users.begin(), users.end(), [](const GpuUser &a, const GpuUser &b) { return a.pid < b.pid; });
    return users;
}

} // namespace AginoGpu
