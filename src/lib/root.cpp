// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/root.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace AginoGpu {

Root::Root(const QString &prefix)
    : m_prefix(prefix.endsWith(QLatin1Char('/')) ? prefix.chopped(1) : prefix)
{
}

Root Root::fromEnvironment()
{
    return Root(qEnvironmentVariable("AGINO_GPU_ROOT"));
}

QString Root::path(const QString &absolute) const
{
    return m_prefix + absolute;
}

std::optional<QString> Root::readTrimmed(const QString &absolute) const
{
    QFile f(path(absolute));
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    return QString::fromUtf8(f.readAll()).trimmed();
}

bool Root::exists(const QString &absolute) const
{
    return QFileInfo::exists(path(absolute));
}

QStringList Root::list(const QString &absolute) const
{
    return QDir(path(absolute)).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System,
                                          QDir::Name);
}

QString Root::linkTarget(const QString &absolute) const
{
    const QFileInfo info(path(absolute));
    if (!info.isSymLink())
        return QString();
    QString target = info.symLinkTarget(); // absolute, resolved inside the prefix tree
    if (!m_prefix.isEmpty() && target.startsWith(m_prefix + QLatin1Char('/')))
        target = target.mid(m_prefix.size());
    return QDir::cleanPath(target);
}

bool Root::writeFile(const QString &absolute, const QByteArray &contents, int mode,
                     QString *error) const
{
    const QString target = path(absolute);
    if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
        if (error)
            *error = QStringLiteral("cannot create the directory of %1").arg(target);
        return false;
    }
    QSaveFile f(target);
    if (!f.open(QIODevice::WriteOnly) || f.write(contents) != contents.size()) {
        if (error)
            *error = QStringLiteral("cannot write %1: %2").arg(target, f.errorString());
        return false;
    }
    QFileDevice::Permissions perms;
    const QFileDevice::Permission bits[9] = {
        QFileDevice::ExeOther, QFileDevice::WriteOther, QFileDevice::ReadOther,
        QFileDevice::ExeGroup, QFileDevice::WriteGroup, QFileDevice::ReadGroup,
        QFileDevice::ExeOwner, QFileDevice::WriteOwner, QFileDevice::ReadOwner,
    };
    for (int i = 0; i < 9; ++i) {
        if (mode & (1 << i))
            perms |= bits[i];
    }
    if (perms & QFileDevice::ReadOwner)
        perms |= QFileDevice::ReadUser;
    if (perms & QFileDevice::WriteOwner)
        perms |= QFileDevice::WriteUser;
    if (perms & QFileDevice::ExeOwner)
        perms |= QFileDevice::ExeUser;
    f.setPermissions(perms);
    if (!f.commit()) {
        if (error)
            *error = QStringLiteral("cannot write %1: %2").arg(target, f.errorString());
        return false;
    }
    return true;
}

bool Root::removeFile(const QString &absolute, QString *error) const
{
    QFile f(path(absolute));
    if (!f.exists() || f.remove())
        return true;
    if (error)
        *error = QStringLiteral("cannot remove %1: %2").arg(f.fileName(), f.errorString());
    return false;
}

} // namespace AginoGpu
