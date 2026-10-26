// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu-client/damsync.h"

#include "agino-gpu-client/gpustate.h"
#include "agino-gpu-client/powerwatcher.h"
#include "agino-gpu-client/rulestore.h"
#include "agino-gpu/rules.h"

#include <DConfig>

#include <QTimer>

#include <memory>

namespace AginoGpu {

namespace {

// dde-application-manager 1.2.59 ApplicationService::appendExtraEnvironments
const QString kDamAppId = QStringLiteral("org.deepin.dde.application-manager");
const QString kDamConfig = QStringLiteral("org.deepin.dde.application-manager");
const QString kDamEnvKey = QStringLiteral("appExtraEnvironments");

// Several triggers often fire together (mode files rewritten, power source switching).
constexpr int kSyncDelayMs = 200;

bool applyUpdate(const DamUpdate &u)
{
    std::unique_ptr<Dtk::Core::DConfig> config(
        Dtk::Core::DConfig::create(kDamAppId, kDamConfig, QLatin1Char('/') + u.appId));
    if (!config->isValid()) {
        qWarning("agino-gpu: DAM config for %s is not available", qPrintable(u.appId));
        return false;
    }
    const QStringList existing = config->value(kDamEnvKey).toStringList();
    QStringList next = removeRuleEnvironment(existing, u.remove);
    if (!u.add.isEmpty())
        next = mergeRuleEnvironment(next, u.add);
    if (next == existing)
        return true;
    if (next.isEmpty())
        config->reset(kDamEnvKey);
    else
        config->setValue(kDamEnvKey, next);
    return true;
}

} // namespace

QList<DamUpdate> planDamSync(const QStringList &appliedApps, const QStringList &appliedEnv,
                             const QStringList &desiredApps, const QStringList &desiredEnv)
{
    QStringList all = appliedApps + desiredApps;
    all.removeDuplicates();
    all.sort();

    QList<DamUpdate> updates;
    for (const QString &app : std::as_const(all)) {
        const bool applied = appliedApps.contains(app);
        const bool desired = desiredApps.contains(app) && !desiredEnv.isEmpty();
        if (applied && desired && appliedEnv == desiredEnv)
            continue;
        if (!applied && !desired)
            continue;
        updates.append(DamUpdate{app, applied ? appliedEnv : QStringList(),
                                 desired ? desiredEnv : QStringList()});
    }
    return updates;
}

bool rulesInForce(bool hybridActive, bool hasOffloadEnv, bool pauseOnBattery, bool onBattery)
{
    return hybridActive && hasOffloadEnv && !(pauseOnBattery && onBattery);
}

DamRuleSync::DamRuleSync(GpuState *state, RuleStore *rules, PowerWatcher *power, QObject *parent)
    : QObject(parent)
    , m_state(state)
    , m_rules(rules)
    , m_power(power)
{
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(kSyncDelayMs);
    connect(timer, &QTimer::timeout, this, &DamRuleSync::sync);
    const auto schedule = [timer] { timer->start(); };
    connect(m_state, &GpuState::changed, timer, schedule);
    connect(m_rules, &RuleStore::rulesChanged, timer, schedule);
    connect(m_power, &PowerWatcher::onBatteryChanged, timer, schedule);
    timer->start();
}

void DamRuleSync::sync()
{
    if (!m_rules->isValid())
        return;
    const RuleSet rules = m_rules->rules();
    const QStringList env = m_state->offloadEnvironment();
    const bool inForce = rulesInForce(m_state->activeMode() == Mode::Hybrid, !env.isEmpty(),
                                      rules.pauseOnBattery, m_power->onBattery());

    QStringList desiredApps;
    QStringList desiredPaths;
    if (inForce) {
        for (const AppRule &a : rules.apps)
            desiredApps << a.appId;
        for (const PathRule &p : rules.paths)
            desiredPaths << p.path;
    }
    const QStringList desiredEnv = inForce ? env : QStringList();

    const QStringList appliedApps = m_rules->appliedApps();
    const QStringList appliedEnv = m_rules->appliedEnvironment();
    QStringList nowApplied = appliedApps;
    bool failed = false;
    for (const DamUpdate &u : planDamSync(appliedApps, appliedEnv, desiredApps, desiredEnv)) {
        if (!applyUpdate(u)) {
            failed = true; // keeps its old entries and stays in appliedApps; retried next time
            continue;
        }
        nowApplied.removeAll(u.appId);
        if (!u.add.isEmpty())
            nowApplied << u.appId;
    }
    nowApplied.sort();

    // After a failure some apps carry the old entries and some the new ones; remembering both
    // lets the next sync take either out (removal only matches exact entries of ours).
    QStringList recordedEnv = desiredEnv;
    if (failed) {
        recordedEnv = appliedEnv + desiredEnv;
        recordedEnv.removeDuplicates();
    }
    m_rules->setApplied(nowApplied, recordedEnv, desiredPaths);
}

} // namespace AginoGpu
