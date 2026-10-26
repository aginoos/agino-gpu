// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace AginoGpu {

class GpuState;
class RuleStore;

// One desktop notification per boot failure the user has not dismissed yet. The notification
// counts as dismissed when it is closed or its action is used.
class FailureNotifier : public QObject
{
    Q_OBJECT
public:
    FailureNotifier(GpuState *state, RuleStore *rules, QObject *parent = nullptr);

    void notifyIfNeeded();

private Q_SLOTS:
    void onActionInvoked(uint id, const QString &actionKey);
    void onClosed(uint id, uint reason);

private:
    void send(const QString &failure);
    void dismiss();

    GpuState *m_state;
    RuleStore *m_rules;
    uint m_id = 0;
    QString m_failure;
};

} // namespace AginoGpu
