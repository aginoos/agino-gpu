// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gpubanner.h"

#include "agino-gpu-client/gpucontrol.h"
#include "agino-gpu-client/gpustate.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace AginoGpu {

class AddAppModel;
class AppCatalog;
class DamRuleSync;
class PowerWatcher;
class RuleStore;

// Everything the control-center "Graphics" page shows and does, exported to QML as dccData.
// Lists are plain QVariantLists of maps; they are small and rebuilt on every change.
class GpuPage : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool multiGpu READ multiGpu NOTIFY stateChanged)
    Q_PROPERTY(QVariantList gpus READ gpus NOTIFY stateChanged)
    Q_PROPERTY(QVariantList modes READ modes NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap banner READ banner NOTIFY bannerChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY bannerChanged)
    Q_PROPERTY(bool rulesEnabled READ rulesEnabled NOTIFY stateChanged)
    Q_PROPERTY(QVariantList apps READ apps NOTIFY rulesChanged)
    Q_PROPERTY(QVariantList paths READ paths NOTIFY rulesChanged)
    Q_PROPERTY(bool pauseOnBattery READ pauseOnBattery WRITE setPauseOnBattery NOTIFY rulesChanged)
    Q_PROPERTY(QObject *addApp READ addApp CONSTANT)

public:
    explicit GpuPage(QObject *parent = nullptr);
    ~GpuPage() override;

    bool multiGpu() const;
    QVariantList gpus() const;
    QVariantList modes() const;
    QVariantMap banner() const { return m_banner; }
    bool busy() const { return m_busy; }
    bool rulesEnabled() const;
    QVariantList apps() const { return m_apps; }
    QVariantList paths() const { return m_paths; }
    bool pauseOnBattery() const { return m_pauseOnBattery; }
    void setPauseOnBattery(bool pause);
    QObject *addApp() const;

    // The page is on screen: poll the discrete GPU's status while it is.
    Q_INVOKABLE void setPageShown(bool shown);

    Q_INVOKABLE void selectMode(const QString &modeId);
    Q_INVOKABLE void bannerAction(const QString &actionId);

    Q_INVOKABLE void removeApp(const QString &appId);
    Q_INVOKABLE void removePath(const QString &path);

Q_SIGNALS:
    void stateChanged();
    void bannerChanged();
    void rulesChanged();

private:
    void onStateChanged();
    void refreshBanner();
    void refreshRules();
    void checkAndSwitch(Mode mode);
    void applyMode(Mode mode, bool force);
    void finishSwitch(Mode mode, const SetModeResult &result);
    void setFlow(FlowNotice::Kind kind, const QString &text, Mode mode);
    void clearFlow();
    void dismissFailure();
    QStringList dgpuOutputs() const;

    GpuState *m_state;
    GpuControl *m_control;
    RuleStore *m_rules;
    PowerWatcher *m_power;
    DamRuleSync *m_sync;
    AppCatalog *m_catalog;
    AddAppModel *m_addApp;

    FlowNotice m_flow;
    QVariantMap m_banner;
    QVariantList m_apps;
    QVariantList m_paths;
    bool m_pauseOnBattery = true;
    bool m_busy = false;
    int m_shown = 0; // pages on screen; DCC may create the new one before destroying the old
};

} // namespace AginoGpu
