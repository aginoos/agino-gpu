// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/chromium.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace AginoGpu {

namespace {

bool isKnownName(const QString &name)
{
    static const QStringList names{
        QStringLiteral("chromium"), QStringLiteral("chromium-browser"), QStringLiteral("chrome"),
        QStringLiteral("google-chrome"), QStringLiteral("google-chrome-stable"),
        QStringLiteral("google-chrome-beta"), QStringLiteral("google-chrome-unstable"),
        QStringLiteral("msedge"), QStringLiteral("microsoft-edge"),
        QStringLiteral("microsoft-edge-stable"), QStringLiteral("microsoft-edge-beta"),
        QStringLiteral("microsoft-edge-dev"), QStringLiteral("brave"), QStringLiteral("brave-browser"),
        QStringLiteral("vivaldi"), QStringLiteral("vivaldi-stable"), QStringLiteral("opera"),
        QStringLiteral("electron"),
    };
    static const QRegularExpression versionedElectron(QStringLiteral("^electron[0-9]+$"));
    return names.contains(name) || versionedElectron.match(name).hasMatch();
}

bool isElectronApp(const Root &root, const QString &dir)
{
    for (const QString &base : {dir, dir + QLatin1String("/..")}) {
        for (const char *rel : {"/resources/app.asar", "/resources/app/package.json"}) {
            if (root.exists(QDir::cleanPath(base + QLatin1String(rel))))
                return true;
        }
    }
    return false;
}

bool hasOffloadVariable(const QStringList &environment)
{
    return environment.contains(QStringLiteral("__NV_PRIME_RENDER_OFFLOAD=1"));
}

bool hasAngleSwitch(const QStringList &args)
{
    for (const QString &arg : args) {
        if (arg.startsWith(QLatin1String("--use-angle=")))
            return true;
    }
    return false;
}

} // namespace

bool isChromiumLike(const Root &root, const QString &canonicalProgram)
{
    const QFileInfo info(canonicalProgram);
    return isKnownName(info.fileName()) || isElectronApp(root, info.path());
}

QStringList chromiumOffloadArguments()
{
    return {QStringLiteral("--use-angle=vulkan"),
            QStringLiteral("--enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan")};
}

QStringList withChromiumOffloadArguments(const Root &root, const QString &canonicalProgram,
                                         const QStringList &args, const QStringList &environment)
{
    if (!hasOffloadVariable(environment) || hasAngleSwitch(args)
        || !isChromiumLike(root, canonicalProgram))
        return args;
    return chromiumOffloadArguments() + args;
}

} // namespace AginoGpu
