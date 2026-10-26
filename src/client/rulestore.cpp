// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu-client/rulestore.h"

#include <DConfig>

#include <QDir>
#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace AginoGpu {

namespace {

const QString kAppId = QStringLiteral("agino-gpu");
const QString kConfigName = QStringLiteral("org.agino.gpu");

const QString kRulesKey = QStringLiteral("rules");
const QString kEffectivePathsKey = QStringLiteral("effectivePaths");
const QString kAppliedAppsKey = QStringLiteral("appliedApps");
const QString kAppliedEnvKey = QStringLiteral("appliedEnvironment");
const QString kDismissedFailureKey = QStringLiteral("dismissedFailure");

// Only entries we wrote are ever deleted.
bool isOurLauncher(const QString &file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return f.readAll().contains("\nX-Agino-Gpu-Rule=true\n");
}

bool writeLauncher(const QString &file, const QByteArray &contents)
{
    if (!QDir().mkpath(QFileInfo(file).path()))
        return false;
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(contents);
    return f.commit();
}

} // namespace

RuleStore::RuleStore(QObject *parent)
    : QObject(parent)
    , m_config(Dtk::Core::DConfig::create(kAppId, kConfigName, QString(), this))
{
    connect(m_config, &Dtk::Core::DConfig::valueChanged, this, [this](const QString &key) {
        if (key == kRulesKey)
            Q_EMIT rulesChanged();
    });
}

RuleStore::~RuleStore() = default;

bool RuleStore::isValid() const
{
    return m_config->isValid();
}

RuleSet RuleStore::rules() const
{
    const QByteArray json = m_config->value(kRulesKey).toString().toUtf8();
    if (json.isEmpty())
        return RuleSet();
    std::optional<RuleSet> rules = ruleSetFromJson(json);
    if (!rules) {
        qWarning("agino-gpu: ignoring malformed rules in DConfig %s", qPrintable(kConfigName));
        return RuleSet();
    }
    return *rules;
}

void RuleStore::setRules(const RuleSet &rules)
{
    m_config->setValue(kRulesKey, QString::fromUtf8(ruleSetToJson(rules)));
}

void RuleStore::addApps(const QStringList &appIds)
{
    RuleSet r = rules();
    bool added = false;
    for (const QString &id : appIds) {
        const bool known = std::any_of(r.apps.cbegin(), r.apps.cend(),
                                       [&id](const AppRule &a) { return a.appId == id; });
        if (!id.isEmpty() && !known) {
            r.apps.append(AppRule{id});
            added = true;
        }
    }
    if (added)
        setRules(r);
}

void RuleStore::removeApp(const QString &appId)
{
    RuleSet r = rules();
    const auto removed = r.apps.removeIf([&appId](const AppRule &a) { return a.appId == appId; });
    if (removed > 0)
        setRules(r);
}

bool RuleStore::addPath(const QString &path, const QString &launcherName, bool createLauncher)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty() || !QFileInfo(canonical).isFile())
        return false;

    RuleSet r = rules();
    for (const PathRule &p : std::as_const(r.paths)) {
        if (p.path == canonical)
            return true;
    }

    PathRule rule{canonical, QString()};
    if (createLauncher) {
        const QString entry = launcherEntryName(canonical);
        const QString name = launcherName.isEmpty() ? QFileInfo(canonical).fileName() : launcherName;
        if (writeLauncher(launcherDirectory() + QLatin1Char('/') + entry,
                          launcherEntryContents(canonical, name)))
            rule.launcherEntry = entry;
        else
            qWarning("agino-gpu: could not write launcher entry %s", qPrintable(entry));
    }
    r.paths.append(rule);
    setRules(r);
    return true;
}

void RuleStore::removePath(const QString &path)
{
    RuleSet r = rules();
    QString entry;
    const auto removed = r.paths.removeIf([&](const PathRule &p) {
        if (p.path != path)
            return false;
        entry = p.launcherEntry;
        return true;
    });
    if (removed == 0)
        return;
    if (!entry.isEmpty()) {
        const QString file = launcherDirectory() + QLatin1Char('/') + entry;
        if (isOurLauncher(file))
            QFile::remove(file);
    }
    setRules(r);
}

void RuleStore::setPauseOnBattery(bool pause)
{
    RuleSet r = rules();
    if (r.pauseOnBattery == pause)
        return;
    r.pauseOnBattery = pause;
    setRules(r);
}

QStringList RuleStore::appliedApps() const
{
    return m_config->value(kAppliedAppsKey).toStringList();
}

QStringList RuleStore::appliedEnvironment() const
{
    return m_config->value(kAppliedEnvKey).toStringList();
}

QStringList RuleStore::effectivePaths() const
{
    return m_config->value(kEffectivePathsKey).toStringList();
}

void RuleStore::setApplied(const QStringList &apps, const QStringList &environment, const QStringList &paths)
{
    if (apps != appliedApps())
        m_config->setValue(kAppliedAppsKey, apps);
    if (environment != appliedEnvironment())
        m_config->setValue(kAppliedEnvKey, environment);
    if (paths != effectivePaths())
        m_config->setValue(kEffectivePathsKey, paths);
}

QString RuleStore::dismissedFailure() const
{
    return m_config->value(kDismissedFailureKey).toString();
}

void RuleStore::setDismissedFailure(const QString &failure)
{
    m_config->setValue(kDismissedFailureKey, failure);
}

QString RuleStore::launcherDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QLatin1String("/applications");
}

} // namespace AginoGpu
