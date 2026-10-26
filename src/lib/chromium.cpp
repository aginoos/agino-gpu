// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/chromium.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <optional>

namespace AginoGpu {

namespace {

const QStringList &offloadFeatures()
{
    static const QStringList features{QStringLiteral("Vulkan"), QStringLiteral("VulkanFromANGLE"),
                                      QStringLiteral("DefaultANGLEVulkan")};
    return features;
}

// Chromium's switch parser: POSIX prefixes are "--" and "-".
std::optional<QString> switchValue(const QString &arg, const QString &name)
{
    for (const QString &prefix : {QStringLiteral("--"), QStringLiteral("-")}) {
        const QString head = prefix + name + QLatin1Char('=');
        if (arg.startsWith(head))
            return arg.mid(head.size());
    }
    return std::nullopt;
}

// "Name<Trial:param/value" -> "Name"
QString featureName(const QString &item)
{
    // Chromium trims whitespace around each name of a feature list.
    const QString name = item.trimmed();
    const qsizetype end = name.indexOf(QRegularExpression(QStringLiteral("[<:]")));
    return end < 0 ? name : name.left(end);
}

} // namespace

bool hasChromiumRuntime(const Root &root, const QString &dir)
{
    for (const char *name : {"icudtl.dat", "resources.pak", "v8_context_snapshot.bin"}) {
        if (!root.exists(QDir::cleanPath(dir + QLatin1Char('/') + QLatin1String(name))))
            return false;
    }
    return true;
}

bool isChromiumLike(const Root &root, const QString &canonicalProgram)
{
    const QString dir = QFileInfo(canonicalProgram).path();
    return hasChromiumRuntime(root, dir) || hasChromiumRuntime(root, dir + QLatin1String("/.."));
}

bool isAppImage(const Root &root, const QString &path)
{
    QFile file(root.path(path));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray header = file.read(11);
    return header.size() == 11 && header.startsWith("\x7f" "ELF") && header[8] == 'A'
        && header[9] == 'I' && header[10] == 0x02;
}

QStringList withChromiumOffloadArguments(const QStringList &args)
{
    qsizetype end = args.indexOf(QStringLiteral("--"));
    if (end < 0)
        end = args.size();

    qsizetype lastEnable = -1;
    std::optional<QString> lastDisable;
    for (qsizetype i = 0; i < end; ++i) {
        if (switchValue(args.at(i), QStringLiteral("use-angle")))
            return args;
        if (switchValue(args.at(i), QStringLiteral("enable-features")))
            lastEnable = i;
        if (const auto value = switchValue(args.at(i), QStringLiteral("disable-features")))
            lastDisable = value;
    }
    if (lastDisable) {
        for (const QString &item : lastDisable->split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            if (offloadFeatures().contains(featureName(item)))
                return args;
        }
    }

    QStringList result = args;
    bool needFeatureSwitch = true;
    if (lastEnable >= 0) {
        const QString arg = args.at(lastEnable);
        const QString prefix = arg.startsWith(QLatin1String("--")) ? QStringLiteral("--")
                                                                   : QStringLiteral("-");
        QStringList items = switchValue(arg, QStringLiteral("enable-features"))
                                ->split(QLatin1Char(','), Qt::SkipEmptyParts);
        QStringList names;
        for (const QString &item : items)
            names << featureName(item);
        for (const QString &feature : offloadFeatures()) {
            if (!names.contains(feature))
                items << feature;
        }
        result[lastEnable] = prefix + QLatin1String("enable-features=") + items.join(QLatin1Char(','));
        needFeatureSwitch = false;
    }
    if (needFeatureSwitch)
        result.prepend(QLatin1String("--enable-features=") + offloadFeatures().join(QLatin1Char(',')));
    result.prepend(QStringLiteral("--use-angle=vulkan"));
    return result;
}

} // namespace AginoGpu
