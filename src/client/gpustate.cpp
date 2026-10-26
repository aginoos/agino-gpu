// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu-client/gpustate.h"

#include "agino-gpu/configgen.h"
#include "agino-gpu/detect.h"
#include "agino-gpu/mode.h"
#include "agino-gpu/offloadenv.h"

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

namespace AginoGpu {

namespace {

constexpr int kPollIntervalMs = 2000;
// A missing state directory (/run/agino-gpu before agino-gpu-apply ran, /var/lib/agino-gpu
// before the first failure) cannot be watched; look for it this often.
constexpr int kRewatchIntervalMs = 30000;

// Directories, not files: the files are replaced by rename (Root::writeFile), which a file
// watch does not survive.
QStringList watchedDirectories()
{
    QStringList dirs;
    for (const char *file : {kModePath, kStatePath, kOffloadEnvPath, kLastFailurePath})
        dirs << QFileInfo(QLatin1String(file)).path();
    dirs.removeDuplicates();
    return dirs;
}

} // namespace

GpuState::GpuState(QObject *parent)
    : GpuState(Root::fromEnvironment(), parent)
{
}

GpuState::GpuState(const Root &root, QObject *parent)
    : QObject(parent)
    , m_root(root)
    , m_watcher(new QFileSystemWatcher(this))
    , m_poll(new QTimer(this))
    , m_rewatch(new QTimer(this))
{
    init();
}

void GpuState::init()
{
    m_poll->setInterval(kPollIntervalMs);
    connect(m_poll, &QTimer::timeout, this, &GpuState::poll);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &GpuState::reload);
    m_rewatch->setInterval(kRewatchIntervalMs);
    connect(m_rewatch, &QTimer::timeout, this, &GpuState::reload);
    m_hw = detectHardware(m_root);
    reload();
}

bool GpuState::isMultiGpu() const
{
    return m_hw.hardwareClass != HardwareClass::Single && !offeredModes().isEmpty();
}

PendingAction GpuState::pendingAction() const
{
    return AginoGpu::pendingAction(m_configured, m_active);
}

QList<Mode> GpuState::offeredModes() const
{
    return AginoGpu::offeredModes(m_hw, m_active);
}

DgpuStatus GpuState::dgpuStatus() const
{
    const Gpu *gpu = m_hw.discreteGpu();
    if (!gpu)
        return m_active == Mode::Integrated ? DgpuStatus::Off : DgpuStatus::None;
    if (gpu->runtimeStatus == QLatin1String("suspended"))
        return DgpuStatus::Suspended;
    return DgpuStatus::Active;
}

void GpuState::setPolling(bool enabled)
{
    m_pollers = qMax(0, m_pollers + (enabled ? 1 : -1));
    if (m_pollers > 0 && !m_poll->isActive()) {
        poll();
        m_poll->start();
    } else if (m_pollers == 0) {
        m_poll->stop();
    }
}

void GpuState::reload()
{
    reloadFiles(false);
}

void GpuState::reloadFiles(bool forceSignal)
{
    const Mode configured = readConfiguredMode(m_root);
    const std::optional<BootState> boot = readBootState(m_root);
    const Mode active = boot ? boot->active : Mode::Hybrid;
    QString failure = boot ? boot->failure : QString();
    std::optional<Mode> failedMode;
    if (!failure.isEmpty())
        failedMode = boot->attempted;
    else
        failure = readLastFailure(m_root);
    const QStringList env = readOffloadEnv(m_root);

    const bool same = configured == m_configured && active == m_active && failure == m_lastFailure
        && env == m_offloadEnv && failedMode == m_failedMode;
    m_configured = configured;
    m_active = active;
    m_lastFailure = failure;
    m_failedMode = failedMode;
    m_offloadEnv = env;
    rewatch();
    if (!same || forceSignal)
        Q_EMIT changed();
}

void GpuState::redetect()
{
    m_hw = detectHardware(m_root);
    reloadFiles(true);
}

void GpuState::rewatch()
{
    // A directory that does not exist yet is picked up by m_rewatch; one that is deleted drops
    // out of the watcher by itself and is picked up the same way when it comes back.
    QStringList toAdd;
    bool absent = false;
    const QStringList watched = m_watcher->directories();
    for (const QString &dir : watchedDirectories()) {
        const QString path = m_root.path(dir);
        if (!QFileInfo::exists(path))
            absent = true;
        else if (!watched.contains(path))
            toAdd << path;
    }
    if (!toAdd.isEmpty())
        m_watcher->addPaths(toAdd);
    if (absent && !m_rewatch->isActive())
        m_rewatch->start();
    else if (!absent)
        m_rewatch->stop();
}

void GpuState::poll()
{
    if (m_hw.discreteIndex < 0)
        return;
    Gpu &gpu = m_hw.gpus[m_hw.discreteIndex];
    const QString status = gpu.runtimeStatus;
    const QStringList outputs = gpu.connectedOutputs;
    refreshGpu(m_root, gpu);
    if (gpu.runtimeStatus != status || gpu.connectedOutputs != outputs)
        Q_EMIT dgpuStatusChanged();
}

} // namespace AginoGpu
