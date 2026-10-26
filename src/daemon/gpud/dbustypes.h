// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusArgument>
#include <QString>

namespace AginoGpu {

// a(ss): (level, "code: message")
struct CheckEntry
{
    QString level; // "info" / "warning" / "error"
    QString text;
};

// a(uus): (pid, uid, name)
struct UserEntry
{
    quint32 pid = 0;
    quint32 uid = 0;
    QString name;
};

QDBusArgument &operator<<(QDBusArgument &arg, const CheckEntry &e);
const QDBusArgument &operator>>(const QDBusArgument &arg, CheckEntry &e);
QDBusArgument &operator<<(QDBusArgument &arg, const UserEntry &e);
const QDBusArgument &operator>>(const QDBusArgument &arg, UserEntry &e);

void registerDBusTypes();

} // namespace AginoGpu
