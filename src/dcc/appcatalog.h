// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QString>

namespace AginoGpu {

struct AppEntry
{
    QString id;   // desktop id without ".desktop"
    QString name; // localized
    QString icon; // icon theme name; may be empty
};

// The installed applications as dde-application-manager lists them on the session bus, read
// with one GetManagedObjects call (NoDisplay entries skipped, sorted by name).
class AppCatalog : public QObject
{
    Q_OBJECT
public:
    explicit AppCatalog(QObject *parent = nullptr);

    const QList<AppEntry> &entries() const { return m_entries; }
    // Display name and icon for a rule's app id; the id itself when DAM does not know it.
    AppEntry lookup(const QString &appId) const;

    void reload();

Q_SIGNALS:
    void changed();

private:
    void request(bool desktopspecManager);
    void apply(QList<AppEntry> entries);

    QList<AppEntry> m_entries;
    bool m_loading = false;
};

} // namespace AginoGpu
