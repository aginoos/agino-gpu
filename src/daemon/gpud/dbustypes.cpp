// SPDX-License-Identifier: GPL-3.0-or-later
#include "dbustypes.h"

#include <QDBusMetaType>
#include <QList>
#include <QMap>
#include <QVariantMap>

namespace AginoGpu {

QDBusArgument &operator<<(QDBusArgument &arg, const CheckEntry &e)
{
    arg.beginStructure();
    arg << e.level << e.text;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, CheckEntry &e)
{
    arg.beginStructure();
    arg >> e.level >> e.text;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const UserEntry &e)
{
    arg.beginStructure();
    arg << e.pid << e.uid << e.name;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, UserEntry &e)
{
    arg.beginStructure();
    arg >> e.pid >> e.uid >> e.name;
    arg.endStructure();
    return arg;
}

void registerDBusTypes()
{
    qDBusRegisterMetaType<CheckEntry>();
    qDBusRegisterMetaType<QList<CheckEntry>>();
    qDBusRegisterMetaType<UserEntry>();
    qDBusRegisterMetaType<QList<UserEntry>>();
    qDBusRegisterMetaType<QList<QVariantMap>>();
    qDBusRegisterMetaType<QMap<QString, QString>>();
}

} // namespace AginoGpu
