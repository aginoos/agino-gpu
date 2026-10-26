// SPDX-License-Identifier: GPL-3.0-or-later
#include "appcatalog.h"

#include <QCollator>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLocale>

#include <algorithm>

namespace AginoGpu {

namespace {

using InterfaceMap = QMap<QString, QVariantMap>;
using ObjectMap = QMap<QDBusObjectPath, InterfaceMap>;
using StringMap = QMap<QString, QString>;

// dde-application-manager 1.2.59, api/dbus/org.desktopspec.ApplicationManager1*.xml
const QString kService = QStringLiteral("org.desktopspec.ApplicationManager1");
const QString kPath = QStringLiteral("/org/desktopspec/ApplicationManager1");
const QString kAppInterface = QStringLiteral("org.desktopspec.ApplicationManager1.Application");
const QString kDesktopspecManager = QStringLiteral("org.desktopspec.DBus.ObjectManager");
const QString kFreedesktopManager = QStringLiteral("org.freedesktop.DBus.ObjectManager");
// Key of the main icon in the Icons map (the others are actions).
const QString kMainIconKey = QStringLiteral("Desktop Entry");

// Locale-keyed map ("en_US", "zh_CN", "default"): the best match for the UI language.
QString localized(const StringMap &map)
{
    if (map.isEmpty())
        return QString();
    const QStringList languages = QLocale().uiLanguages();
    for (const QString &language : languages) {
        QString key = language;
        key.replace(QLatin1Char('-'), QLatin1Char('_'));
        if (map.contains(key))
            return map.value(key);
        const QString lang = key.section(QLatin1Char('_'), 0, 0);
        if (map.contains(lang))
            return map.value(lang);
    }
    if (map.contains(QStringLiteral("default")))
        return map.value(QStringLiteral("default"));
    return map.cbegin().value();
}

bool toEntry(const QVariantMap &props, AppEntry *entry)
{
    if (props.value(QStringLiteral("NoDisplay")).toBool())
        return false;
    entry->id = props.value(QStringLiteral("ID")).toString();
    if (entry->id.isEmpty())
        return false;
    entry->name = localized(qdbus_cast<StringMap>(props.value(QStringLiteral("Name"))));
    if (entry->name.isEmpty())
        entry->name = entry->id;
    const StringMap icons = qdbus_cast<StringMap>(props.value(QStringLiteral("Icons")));
    entry->icon = icons.contains(kMainIconKey) ? icons.value(kMainIconKey) : localized(icons);
    return true;
}

QList<AppEntry> parse(const ObjectMap &objects)
{
    QList<AppEntry> entries;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const auto iface = it.value().constFind(kAppInterface);
        AppEntry entry;
        if (iface != it.value().cend() && toEntry(iface.value(), &entry))
            entries.append(entry);
    }
    QCollator collator;
    std::sort(entries.begin(), entries.end(), [&collator](const AppEntry &a, const AppEntry &b) {
        return collator.compare(a.name, b.name) < 0;
    });
    return entries;
}

} // namespace

AppCatalog::AppCatalog(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<InterfaceMap>();
    qDBusRegisterMetaType<ObjectMap>();
    qDBusRegisterMetaType<StringMap>();
    reload();
}

AppEntry AppCatalog::lookup(const QString &appId) const
{
    for (const AppEntry &entry : m_entries) {
        if (entry.id == appId)
            return entry;
    }
    return AppEntry{appId, appId, QString()};
}

void AppCatalog::reload()
{
    if (!m_loading)
        request(true);
}

void AppCatalog::request(bool desktopspecManager)
{
    m_loading = true;
    const QDBusMessage call = QDBusMessage::createMethodCall(
        kService, kPath, desktopspecManager ? kDesktopspecManager : kFreedesktopManager,
        QStringLiteral("GetManagedObjects"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, desktopspecManager](QDBusPendingCallWatcher *) {
                watcher->deleteLater();
                const QDBusPendingReply<ObjectMap> reply = *watcher;
                if (reply.isError() && desktopspecManager) {
                    request(false);
                    return;
                }
                m_loading = false;
                if (reply.isError()) {
                    qWarning("agino-gpu: cannot list applications: %s", qPrintable(reply.error().message()));
                    return;
                }
                apply(parse(reply.value()));
            });
}

void AppCatalog::apply(QList<AppEntry> entries)
{
    m_entries = std::move(entries);
    Q_EMIT changed();
}

} // namespace AginoGpu
