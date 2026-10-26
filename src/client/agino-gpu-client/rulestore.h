// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/rules.h"

#include <QObject>
#include <QStringList>

namespace Dtk::Core {
class DConfig;
}

namespace AginoGpu {

// The user's "always use the discrete GPU" rules (design.md §5.2), kept in the user DConfig
// org.agino.gpu of appid agino-gpu (data/dconfig/org.agino.gpu.json):
//   rules              JSON RuleSet (rules.h), edited by the control center
//   effectivePaths     canonical paths of the path rules in force right now; written by
//                      DamRuleSync, read by the DAM patch (empty = no path rule applies)
//   appliedApps        app ids whose DAM appExtraEnvironments carry our entries
//   appliedEnvironment the KEY=VALUE entries we put there (needed to take them out again)
//   dismissedFailure   last boot-failure text the user dismissed (quick-panel notification)
class RuleStore : public QObject
{
    Q_OBJECT
public:
    explicit RuleStore(QObject *parent = nullptr);
    ~RuleStore() override;

    bool isValid() const;

    RuleSet rules() const;
    void setRules(const RuleSet &rules);

    // Adds app rules, skipping ids that already have one.
    void addApps(const QStringList &appIds);
    void removeApp(const QString &appId);
    // `path` is canonicalized; returns false (and leaves the rules alone) when it is not an
    // existing regular file. createLauncher writes ~/.local/share/applications/<entry>.
    bool addPath(const QString &path, const QString &launcherName, bool createLauncher);
    // Also removes the launcher entry we created for it.
    void removePath(const QString &path);
    void setPauseOnBattery(bool pause);

    QStringList appliedApps() const;
    QStringList appliedEnvironment() const;
    QStringList effectivePaths() const;
    void setApplied(const QStringList &apps, const QStringList &environment, const QStringList &paths);

    QString dismissedFailure() const;
    void setDismissedFailure(const QString &failure);

    static QString launcherDirectory(); // ~/.local/share/applications

Q_SIGNALS:
    void rulesChanged();

private:
    Dtk::Core::DConfig *m_config = nullptr;
};

} // namespace AginoGpu
