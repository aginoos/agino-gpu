// SPDX-License-Identifier: GPL-3.0-or-later
#include "plugin.h"

#include "agino-gpu-client/damsync.h"
#include "agino-gpu-client/gpustate.h"
#include "agino-gpu-client/powerwatcher.h"
#include "agino-gpu-client/rulestore.h"
#include "agino-gpu-client/texts.h"
#include "constants.h"
#include "failurenotifier.h"
#include "panel.h"
#include "tile.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

using namespace AginoGpu;

namespace {
const QString kItemKey = QStringLiteral("agino-gpu-item");
}

AginoGpuPlugin::AginoGpuPlugin(QObject *parent)
    : QObject(parent)
{
}

AginoGpuPlugin::~AginoGpuPlugin()
{
    delete m_placeholder;
    delete m_tile;
    delete m_panel;
}

const QString AginoGpuPlugin::pluginName() const
{
    return QStringLiteral("agino-gpu");
}

const QString AginoGpuPlugin::pluginDisplayName() const
{
    return Texts::title();
}

void AginoGpuPlugin::loadPluginTranslator()
{
    auto *translator = new QTranslator(this);
    if (translator->load(QLocale(), QStringLiteral("agino-gpu-quickpanel"), QStringLiteral("_"),
                         QStringLiteral(AGINO_GPU_QP_TRANSLATIONS_DIR)))
        QCoreApplication::installTranslator(translator);
    else
        delete translator;
}

void AginoGpuPlugin::init(PluginProxyInterface *proxyInter)
{
    m_proxyInter = proxyInter;

    Texts::installTranslator();
    loadPluginTranslator();

    m_state = new GpuState(this);
    m_rules = new RuleStore(this);
    m_power = new PowerWatcher(this);
    m_dam = new DamRuleSync(m_state, m_rules, m_power, this);
    m_notifier = new FailureNotifier(m_state, m_rules, this);

    connect(m_state, &GpuState::changed, this, &AginoGpuPlugin::syncVisibility);
    syncVisibility();
    m_notifier->notifyIfNeeded();
}

void AginoGpuPlugin::syncVisibility()
{
    if (m_tile)
        m_tile->refresh();

    const bool wanted = m_state->isMultiGpu();
    if (wanted == m_shown)
        return;
    m_shown = wanted;
    if (wanted)
        m_proxyInter->itemAdded(this, kItemKey);
    else
        m_proxyInter->itemRemoved(this, kItemKey);
}

QWidget *AginoGpuPlugin::itemWidget(const QString &itemKey)
{
    if (itemKey == Dock::QUICK_ITEM_KEY) {
        if (!m_tile) {
            m_tile = new Tile(m_state);
            connect(m_tile, &Tile::clicked, this,
                    [this] { m_proxyInter->requestSetAppletVisible(this, kItemKey, true); });
        }
        return m_tile;
    }
    if (itemKey == kItemKey) {
        // Never shown (no tray icon); the dock still asks for a widget.
        if (!m_placeholder)
            m_placeholder = new QWidget;
        return m_placeholder;
    }
    return nullptr;
}

QWidget *AginoGpuPlugin::itemPopupApplet(const QString &itemKey)
{
    if (itemKey != kItemKey)
        return nullptr;
    if (!m_panel) {
        m_panel = new Panel(m_state);
        connect(m_panel, &Panel::requestHide, this,
                [this] { m_proxyInter->requestSetAppletVisible(this, kItemKey, false); });
    }
    return m_panel;
}
