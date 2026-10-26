// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpupage.h"

#include "addappmodel.h"
#include "appcatalog.h"

#include "agino-gpu-client/damsync.h"
#include "agino-gpu-client/powerwatcher.h"
#include "agino-gpu-client/rulestore.h"
#include "agino-gpu-client/texts.h"
#include "agino-gpu/mode.h"

#include <QDir>
#include <QFileInfo>

namespace AginoGpu {

namespace {

const QString kDefaultAppIcon = QStringLiteral("application-x-executable");

QString abbreviateHome(const QString &dir)
{
    const QString home = QDir::homePath();
    if (dir == home || dir.startsWith(home + QLatin1Char('/')))
        return QLatin1Char('~') + dir.mid(home.size());
    return dir;
}

QString gpuName(const Gpu &gpu)
{
    return gpu.name.isEmpty() ? gpu.pciAddress : gpu.name;
}

QString statusKind(DgpuStatus status)
{
    switch (status) {
    case DgpuStatus::Active:
        return QStringLiteral("active");
    case DgpuStatus::Suspended:
        return QStringLiteral("sleep");
    case DgpuStatus::Off:
        return QStringLiteral("off");
    case DgpuStatus::None:
        break;
    }
    return QString();
}

} // namespace

GpuPage::GpuPage(QObject *parent)
    : QObject(parent)
    , m_state(new GpuState(this))
    , m_control(new GpuControl(this))
    , m_rules(new RuleStore(this))
    , m_power(new PowerWatcher(this))
    , m_sync(new DamRuleSync(m_state, m_rules, m_power, this))
    , m_catalog(new AppCatalog(this))
    , m_addApp(new AddAppModel(m_catalog, m_rules, this))
{
    Texts::installTranslator();
    connect(m_state, &GpuState::changed, this, &GpuPage::onStateChanged);
    connect(m_state, &GpuState::dgpuStatusChanged, this, &GpuPage::stateChanged);
    connect(m_rules, &RuleStore::rulesChanged, this, &GpuPage::refreshRules);
    connect(m_catalog, &AppCatalog::changed, this, &GpuPage::refreshRules);
    refreshRules();
    refreshBanner();
}

GpuPage::~GpuPage()
{
    setPageShown(false);
}

bool GpuPage::multiGpu() const
{
    return m_state->isMultiGpu();
}

QObject *GpuPage::addApp() const
{
    return m_addApp;
}

bool GpuPage::rulesEnabled() const
{
    return m_state->activeMode() == Mode::Hybrid;
}

QVariantList GpuPage::gpus() const
{
    QVariantList list;
    const Hardware &hw = m_state->hardware();
    for (int i = 0; i < hw.gpus.size(); ++i) {
        const Gpu &gpu = hw.gpus.at(i);
        const bool discrete = i == hw.discreteIndex;
        // The integrated GPU is invisible while the MUX routes everything through the dGPU.
        if (!discrete && m_state->activeMode() == Mode::MuxDedicated)
            continue;
        QStringList parts{gpu.driver};
        parts.append(gpu.connectedOutputs.join(QLatin1String(", ")));
        parts.removeAll(QString());
        const DgpuStatus status = discrete ? m_state->dgpuStatus() : DgpuStatus::None;
        list.append(QVariantMap{{QStringLiteral("name"), gpuName(gpu)},
                                {QStringLiteral("line"), parts.join(QLatin1String(" · "))},
                                {QStringLiteral("status"), Texts::dgpuStatus(status)},
                                {QStringLiteral("statusKind"), statusKind(status)}});
    }
    return list;
}

QVariantList GpuPage::modes() const
{
    const QList<Mode> offered = m_state->offeredModes();
    QList<Mode> shown = supportedModes(m_state->hardware());
    for (Mode mode : offered + QList<Mode>{m_state->activeMode()}) {
        if (!shown.contains(mode))
            shown.append(mode);
    }
    QVariantList list;
    for (Mode mode : {Mode::Hybrid, Mode::Integrated, Mode::Dedicated, Mode::MuxDedicated}) {
        if (!shown.contains(mode))
            continue;
        const bool selected = mode == m_state->configuredMode();
        const PendingAction cost = AginoGpu::pendingAction(mode, m_state->activeMode());
        list.append(QVariantMap{{QStringLiteral("id"), modeToString(mode)},
                                {QStringLiteral("name"), Texts::modeName(mode)},
                                {QStringLiteral("summary"), Texts::modeSummary(mode)},
                                {QStringLiteral("selected"), selected},
                                {QStringLiteral("cost"), selected ? QString() : Texts::pendingCost(cost)},
                                {QStringLiteral("enabled"), offered.contains(mode)}});
    }
    return list;
}

void GpuPage::setPageShown(bool shown)
{
    if (shown == m_shown)
        return;
    m_shown = shown;
    m_state->setPolling(shown);
}

void GpuPage::onStateChanged()
{
    refreshBanner();
    Q_EMIT stateChanged();
}

QStringList GpuPage::dgpuOutputs() const
{
    const Gpu *dgpu = m_state->discreteGpu();
    return dgpu ? dgpu->connectedOutputs : QStringList();
}

void GpuPage::refreshBanner()
{
    BannerInput in;
    in.configured = m_state->configuredMode();
    in.failedMode = m_state->failedMode();
    in.active = m_state->activeMode();
    in.pending = m_state->pendingAction();
    in.lastFailure = m_state->lastFailure();
    in.failureDismissed = !in.lastFailure.isEmpty() && in.lastFailure == m_rules->dismissedFailure();
    in.flow = m_flow;
    in.dgpuOutputs = dgpuOutputs();
    in.integratedOffered = m_state->offeredModes().contains(Mode::Integrated);
    m_banner = buildBanner(in);
    Q_EMIT bannerChanged();
}

void GpuPage::refreshRules()
{
    const RuleSet rules = m_rules->rules();
    m_pauseOnBattery = rules.pauseOnBattery;
    m_apps.clear();
    for (const AppRule &rule : rules.apps) {
        const AppEntry entry = m_catalog->lookup(rule.appId);
        m_apps.append(QVariantMap{{QStringLiteral("appId"), rule.appId},
                                  {QStringLiteral("name"), entry.name},
                                  {QStringLiteral("icon"), entry.icon.isEmpty() ? kDefaultAppIcon : entry.icon}});
    }
    m_paths.clear();
    for (const PathRule &rule : rules.paths) {
        const QFileInfo info(rule.path);
        m_paths.append(QVariantMap{{QStringLiteral("path"), rule.path},
                                   {QStringLiteral("name"), info.fileName()},
                                   {QStringLiteral("directory"), abbreviateHome(info.absolutePath())},
                                   {QStringLiteral("exists"), info.isFile()},
                                   {QStringLiteral("icon"), kDefaultAppIcon}});
    }
    Q_EMIT rulesChanged();
}

void GpuPage::setPauseOnBattery(bool pause)
{
    if (pause != m_pauseOnBattery)
        m_rules->setPauseOnBattery(pause);
}

void GpuPage::removeApp(const QString &appId)
{
    m_rules->removeApp(appId);
}

void GpuPage::removePath(const QString &path)
{
    m_rules->removePath(path);
}

} // namespace AginoGpu
