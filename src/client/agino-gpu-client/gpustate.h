// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/root.h"
#include "agino-gpu/types.h"

#include <QObject>
#include <QStringList>

#include <optional>

class QFileSystemWatcher;
class QTimer;

namespace AginoGpu {

// What the discrete GPU is doing right now (quick panel sub-page, control-center GPU list).
enum class DgpuStatus { None, Off, Suspended, Active };

// The UI's view of the machine, read straight from sysfs and the state files (design.md §6.1:
// state does not go through the daemon, so watching it never activates agino-gpud). Hardware
// is detected once; the mode files are watched; the volatile GPU fields (runtime status,
// connected outputs) are polled only while someone asks for it (setPolling).
class GpuState : public QObject
{
    Q_OBJECT
public:
    explicit GpuState(QObject *parent = nullptr);
    explicit GpuState(const Root &root, QObject *parent = nullptr);

    const Root &root() const { return m_root; }
    const Hardware &hardware() const { return m_hw; }
    // Whether there is a mode to offer (also true in integrated mode, where the discrete GPU is
    // gone); the plugins hide themselves otherwise.
    bool isMultiGpu() const;

    Mode configuredMode() const { return m_configured; }
    Mode activeMode() const { return m_active; }
    PendingAction pendingAction() const;
    QList<Mode> offeredModes() const;
    // Boot failure of this boot (from /run/agino-gpu/state) or, failing that, the persistent
    // /var/lib/agino-gpu/last-failure text. Empty when there is none.
    QString lastFailure() const { return m_lastFailure; }
    // The mode whose boot failed, when this boot recorded it; nullopt for a failure that only
    // remains in /var/lib/agino-gpu/last-failure from an earlier boot.
    std::optional<Mode> failedMode() const { return m_failedMode; }
    QStringList offloadEnvironment() const { return m_offloadEnv; }

    DgpuStatus dgpuStatus() const;
    const Gpu *discreteGpu() const { return m_hw.discreteGpu(); }

    // Poll the volatile fields every 2 s while enabled (quick-panel sub-page visible,
    // control-center page shown). Reference counted.
    void setPolling(bool enabled);

public Q_SLOTS:
    void reload();         // mode files
    void redetect();       // full hardware detection + mode files

Q_SIGNALS:
    void changed();        // anything above changed
    void dgpuStatusChanged();

private:
    void init();
    void rewatch();
    void reloadFiles(bool forceSignal);
    void poll();

    Root m_root;
    Hardware m_hw;
    Mode m_configured = Mode::Hybrid;
    Mode m_active = Mode::Hybrid;
    QString m_lastFailure;
    std::optional<Mode> m_failedMode;
    QStringList m_offloadEnv;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_poll = nullptr;
    QTimer *m_rewatch = nullptr; // runs while a watched directory does not exist
    int m_pollers = 0;
};

} // namespace AginoGpu
