// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu-client/gpucontrol.h"
#include "agino-gpu-client/gpustate.h"

#include <QObject>
#include <QString>

namespace AginoGpu {

// What the user asked for and how it went: CheckMode first, then SetMode (force once the user
// confirmed warnings). One request at a time. The outcome is either nothing (success or silent
// revert), a blocking message, or a warning waiting for confirmation.
class ModeSwitcher : public QObject
{
    Q_OBJECT
public:
    struct Notice
    {
        QString text;
        bool needsConfirm = false;
        bool isEmpty() const { return text.isEmpty(); }
    };

    ModeSwitcher(GpuState *state, GpuControl *control, QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    const Notice &notice() const { return m_notice; }

    void select(Mode mode);
    void confirm();
    void undo();            // back to the active mode, no checks
    void clearNotice();

Q_SIGNALS:
    void changed();

private:
    void request(Mode mode, bool force);
    void finish(const SetModeResult &result, Mode mode, quint64 generation);
    void setNotice(const QString &text, bool needsConfirm, Mode mode);

    GpuState *m_state;
    GpuControl *m_control;
    bool m_busy = false;
    Notice m_notice;
    Mode m_target = Mode::Hybrid;
    // Bumped by clearNotice() (also when the panel hides): a reply that arrives for an older
    // generation updates the state but sets no notice, so no stale confirm banner reappears.
    quint64 m_generation = 0;
};

} // namespace AginoGpu
