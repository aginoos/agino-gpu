// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QStringList>

namespace AginoGpu {

class GpuState;
class PowerWatcher;
class RuleStore;

// One app whose DAM appExtraEnvironments must change: take `remove` out (only our entries,
// see removeRuleEnvironment), then merge `add` (empty = the rule no longer applies).
struct DamUpdate
{
    QString appId;
    QStringList remove;
    QStringList add;

    bool operator==(const DamUpdate &o) const
    {
        return appId == o.appId && remove == o.remove && add == o.add;
    }
};

// Pure planning step of the sync, unit tested: what changes between what we applied last
// time and what should be applied now. Apps whose entries are already right are skipped.
// Result is sorted by app id.
QList<DamUpdate> planDamSync(const QStringList &appliedApps, const QStringList &appliedEnv,
                             const QStringList &desiredApps, const QStringList &desiredEnv);

// Whether the rules apply right now (design.md §5.2): hybrid mode with an offload.env, and not
// paused because we run on battery.
bool rulesInForce(bool hybridActive, bool hasOffloadEnv, bool pauseOnBattery, bool onBattery);

// Keeps DAM's per-app DConfig (org.deepin.dde.application-manager, subpath /<appid>) in line
// with the rules, the active mode and the power source. The quick-panel plugin owns one for
// the whole session; the control center runs one too so edits apply without the panel.
// sync() is idempotent, so two of them only repeat each other's work.
class DamRuleSync : public QObject
{
    Q_OBJECT
public:
    DamRuleSync(GpuState *state, RuleStore *rules, PowerWatcher *power, QObject *parent = nullptr);

public Q_SLOTS:
    void sync();

private:
    GpuState *m_state;
    RuleStore *m_rules;
    PowerWatcher *m_power;
};

} // namespace AginoGpu
