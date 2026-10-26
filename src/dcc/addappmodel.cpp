// SPDX-License-Identifier: GPL-3.0-or-later
#include "addappmodel.h"

#include "appcatalog.h"

#include "agino-gpu-client/rulestore.h"

#include "agino-gpu/rules.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace AginoGpu {

namespace {

const QString kDesktopSuffix = QStringLiteral("desktop");

} // namespace

AddAppModel::AddAppModel(AppCatalog *catalog, RuleStore *rules, QObject *parent)
    : QAbstractListModel(parent)
    , m_catalog(catalog)
    , m_rules(rules)
{
    connect(m_catalog, &AppCatalog::changed, this, &AddAppModel::rebuild);
    connect(m_rules, &RuleStore::rulesChanged, this, &AddAppModel::refreshAdded);
    refreshAdded();
    rebuild();
}

int AddAppModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant AddAppModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return QVariant();
    const AppEntry &entry = m_catalog->entries().at(m_rows.at(index.row()));
    switch (role) {
    case AppIdRole:
        return entry.id;
    case NameRole:
        return entry.name;
    case IconRole:
        return entry.icon;
    case AlreadyAddedRole:
        return m_added.contains(entry.id);
    case CheckedRole:
        return m_added.contains(entry.id) || m_selected.contains(entry.id);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> AddAppModel::roleNames() const
{
    return {{AppIdRole, "appId"}, {NameRole, "name"}, {IconRole, "icon"},
            {AlreadyAddedRole, "alreadyAdded"}, {CheckedRole, "checked"}};
}

void AddAppModel::setFilter(const QString &filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    rebuild();
    Q_EMIT filterChanged();
}

int AddAppModel::selectedCount() const
{
    return m_selected.size() + (hasFile() ? 1 : 0);
}

QString AddAppModel::fileName() const
{
    return QFileInfo(m_file).fileName();
}

QString AddAppModel::fileDirectory() const
{
    const QString dir = QFileInfo(m_file).absolutePath();
    const QString home = QDir::homePath();
    return dir == home || dir.startsWith(home + QLatin1Char('/')) ? QLatin1Char('~') + dir.mid(home.size()) : dir;
}

bool AddAppModel::fileIsDesktopEntry() const
{
    return QFileInfo(m_file).suffix() == kDesktopSuffix;
}

void AddAppModel::setCreateLauncher(bool create)
{
    if (create == m_createLauncher)
        return;
    m_createLauncher = create;
    Q_EMIT selectionChanged();
}

void AddAppModel::reset()
{
    m_selected.clear();
    m_file.clear();
    m_createLauncher = true;
    m_filter.clear();
    m_catalog->reload();
    refreshAdded();
    rebuild();
    Q_EMIT filterChanged();
    Q_EMIT selectionChanged();
}

void AddAppModel::toggle(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;
    const QString id = m_catalog->entries().at(m_rows.at(row)).id;
    if (m_added.contains(id))
        return;
    if (!m_selected.remove(id))
        m_selected.insert(id);
    Q_EMIT dataChanged(index(row), index(row), {CheckedRole});
    Q_EMIT selectionChanged();
}

void AddAppModel::setFile(const QUrl &file)
{
    const QFileInfo info(file.toLocalFile());
    if (!info.isFile()) {
        qWarning("agino-gpu: %s is not a file", qPrintable(file.toString()));
        return;
    }
    m_file = info.absoluteFilePath();
    Q_EMIT selectionChanged();
}

void AddAppModel::clearFile()
{
    m_file.clear();
    Q_EMIT selectionChanged();
}

namespace {

struct DesktopEntry
{
    QString name;
    QString exec;
};

// Name= and Exec= of the [Desktop Entry] group (unlocalized; enough to resolve the program).
DesktopEntry readDesktopEntry(const QString &file)
{
    DesktopEntry entry;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return entry;
    bool inGroup = false;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('[')))
            inGroup = line == QLatin1String("[Desktop Entry]");
        else if (inGroup && line.startsWith(QLatin1String("Name=")) && entry.name.isEmpty())
            entry.name = line.mid(5);
        else if (inGroup && line.startsWith(QLatin1String("Exec=")) && entry.exec.isEmpty())
            entry.exec = line.mid(5);
    }
    return entry;
}

} // namespace

bool AddAppModel::commit()
{
    QStringList appIds = m_selected.values();
    if (hasFile()) {
        const QFileInfo info(m_file);
        const QString id = info.completeBaseName();
        if (fileIsDesktopEntry() && m_catalog->contains(id)) {
            appIds.append(id);
        } else if (fileIsDesktopEntry()) {
            // DAM does not know this file (outside the application directories): the rule
            // follows the program it starts instead.
            const DesktopEntry entry = readDesktopEntry(m_file);
            const QString program = resolveExecProgram(entry.exec, qEnvironmentVariable("PATH"));
            if (program.isEmpty()
                || !m_rules->addPath(program, entry.name.isEmpty() ? id : entry.name, m_createLauncher))
                return false;
        } else if (!m_rules->addPath(m_file, info.fileName(), m_createLauncher)) {
            return false;
        }
    }
    std::sort(appIds.begin(), appIds.end());
    m_rules->addApps(appIds);
    reset();
    return true;
}

void AddAppModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    const QList<AppEntry> &entries = m_catalog->entries();
    for (int i = 0; i < entries.size(); ++i) {
        if (m_filter.isEmpty() || entries.at(i).name.contains(m_filter, Qt::CaseInsensitive)
            || entries.at(i).id.contains(m_filter, Qt::CaseInsensitive))
            m_rows.append(i);
    }
    endResetModel();
}

void AddAppModel::refreshAdded()
{
    m_added.clear();
    for (const AppRule &rule : m_rules->rules().apps)
        m_added.insert(rule.appId);
    if (!m_rows.isEmpty())
        Q_EMIT dataChanged(index(0), index(m_rows.size() - 1), {AlreadyAddedRole, CheckedRole});
}

} // namespace AginoGpu
