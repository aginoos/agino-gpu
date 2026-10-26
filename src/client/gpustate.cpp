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
{
    init();
}

void GpuState::init()
{
    m_poll->setInterval(kPollIntervalMs);
    connect(m_poll, &QTimer::timeout, this, &GpuState::poll);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &GpuState::reload);
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
    const Mode configured = readConfiguredMode(m_root);
    const std::optional<BootState> boot = readBootState(m_root);
    const Mode active = boot ? boot->active : Mode::Hybrid;
    QString failure = boot ? boot->failure : QString();
    if (failure.isEmpty())
        failure = readLastFailure(m_root);
    const QStringList env = readOffloadEnv(m_root);

    const bool same = configured == m_configured && active == m_active && failure == m_lastFailure
        && env == m_offloadEnv;
    m_configured = configured;
    m_active = active;
    m_lastFailure = failure;
    m_offloadEnv = env;
    rewatch();
    if (!same)
        Q_EMIT changed();
}

void GpuState::redetect()
{
    m_hw = detectHardware(m_root);
    reload();
    Q_EMIT changed();
}

void GpuState::rewatch()
{
    // A directory that did not exist yet (/run/agino-gpu before agino-gpu-apply ran) is
    // picked up on the next reload.
    QStringList missing;
    const QStringList watched = m_watcher->directories();
    for (const QString &dir : watchedDirectories()) {
        const QString path = m_root.path(dir);
        if (!watched.contains(path) && QFileInfo::exists(path))
            missing << path;
    }
    if (!missing.isEmpty())
        m_watcher->addPaths(missing);
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
