// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "pluginsiteminterface_v2.h"

#include <QObject>
#include <QPointer>

namespace AginoGpu {
class DamRuleSync;
class FailureNotifier;
class GpuState;
class Panel;
class PowerWatcher;
class RuleStore;
class Tile;
} // namespace AginoGpu

class QTranslator;

// Quick-panel entry of agino-gpu: a two-cell tile that opens the graphics sub-page. Only
// present on machines with more than one GPU; no tray icon.
class AginoGpuPlugin : public QObject, public PluginsItemInterfaceV2
{
    Q_OBJECT
    Q_INTERFACES(PluginsItemInterfaceV2)
    Q_PLUGIN_METADATA(IID ModuleInterface_iid_V2 FILE "agino-gpu.json")

public:
    explicit AginoGpuPlugin(QObject *parent = nullptr);
    ~AginoGpuPlugin() override;

    const QString pluginName() const override;
    const QString pluginDisplayName() const override;
    void init(PluginProxyInterface *proxyInter) override;
    QWidget *itemWidget(const QString &itemKey) override;
    QWidget *itemPopupApplet(const QString &itemKey) override;
    Dock::PluginFlags flags() const override { return Dock::Type_Quick | Dock::Quick_Panel_Multi; }

private:
    void syncVisibility();
    void loadPluginTranslator();

    AginoGpu::GpuState *m_state = nullptr;
    AginoGpu::RuleStore *m_rules = nullptr;
    AginoGpu::PowerWatcher *m_power = nullptr;
    AginoGpu::DamRuleSync *m_dam = nullptr;
    AginoGpu::FailureNotifier *m_notifier = nullptr;
    QPointer<AginoGpu::Tile> m_tile;
    QPointer<AginoGpu::Panel> m_panel;
    QPointer<QWidget> m_placeholder;
    bool m_shown = false;
};
