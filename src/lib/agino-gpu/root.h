// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace AginoGpu {

// Every absolute path the library reads or writes (/sys, /proc, /etc, /run, /var, /dev) goes
// through a Root, so tests can point the code at a fixture tree. The prefix comes from the
// AGINO_GPU_ROOT environment variable; empty means the real file system.
class Root
{
public:
    explicit Root(const QString &prefix = QString());
    static Root fromEnvironment();

    QString prefix() const { return m_prefix; }
    // "/sys/bus/pci/devices" -> "<prefix>/sys/bus/pci/devices"
    QString path(const QString &absolute) const;

    // Contents with surrounding whitespace removed; nullopt when the file cannot be read.
    std::optional<QString> readTrimmed(const QString &absolute) const;
    bool exists(const QString &absolute) const;
    // Entry names in a directory (no "." or ".."), sorted; empty when it does not exist.
    QStringList list(const QString &absolute) const;
    // Target of a symbolic link as an absolute path *without* the prefix, resolved against the
    // link's directory ("/sys/bus/pci/drivers/nvidia"); empty when it is not a link.
    QString linkTarget(const QString &absolute) const;

    // Atomic write (temporary file + rename), creating parent directories; mode is octal
    // permissions. Returns false and fills error on failure.
    bool writeFile(const QString &absolute, const QByteArray &contents, int mode = 0644,
                   QString *error = nullptr) const;
    // Removes a file; a missing file is success.
    bool removeFile(const QString &absolute, QString *error = nullptr) const;

private:
    QString m_prefix;
};

} // namespace AginoGpu
