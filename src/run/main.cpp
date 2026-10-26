// SPDX-License-Identifier: GPL-3.0-or-later
// agino-gpu-run [--chromium] [--] COMMAND [ARGS...]: run a command on the discrete GPU (design.md §5.1).

#include "agino-gpu/chromium.h"
#include "agino-gpu/offloadenv.h"
#include "agino-gpu/root.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace {

void usage(FILE *out)
{
    std::fputs("Usage: agino-gpu-run [--chromium] [--] COMMAND [ARGS...]\n"
               "Run COMMAND on the discrete GPU (PRIME render offload).\n"
               "When no discrete GPU is available, COMMAND runs unchanged.\n"
               "Chromium and Electron programs (detected by their runtime files, also inside an\n"
               "AppImage) get the ANGLE-on-Vulkan switches they need to render on an NVIDIA GPU.\n"
               "  --chromium  treat COMMAND as Chromium-based without detection (for wrapper scripts)\n",
               out);
}

// The program as exec will find it: PATH lookup when it has no '/', then symlinks resolved.
QString canonicalProgram(const char *program)
{
    QString path = QString::fromLocal8Bit(program);
    if (!path.contains(QLatin1Char('/')))
        path = QStandardPaths::findExecutable(path);
    return path.isEmpty() ? QString() : QFileInfo(path).canonicalFilePath();
}

// An AppImage shows its contents only when mounted: `<appimage> --appimage-mount` prints the mount
// point and keeps it mounted until the process ends. Any failure means "not Chromium-like" and the
// launch stays unchanged (design.md §5.4).
bool appImageIsChromium(const QString &path)
{
    QProcess mount;
    mount.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    mount.start(path, {QStringLiteral("--appimage-mount")});
    if (!mount.waitForStarted(3000))
        return false;
    QByteArray line;
    QElapsedTimer timer;
    timer.start();
    while (!line.contains('\n') && timer.elapsed() < 3000 && mount.state() != QProcess::NotRunning) {
        if (mount.waitForReadyRead(200) || mount.bytesAvailable() > 0)
            line += mount.readAllStandardOutput();
    }
    const QString mountPoint = QString::fromLocal8Bit(line.left(line.indexOf('\n'))).trimmed();
    bool chromium = false;
    if (line.contains('\n') && mountPoint.startsWith(QLatin1Char('/')))
        chromium = AginoGpu::hasChromiumRuntime(AginoGpu::Root(), mountPoint);
    // Ends exactly the child we started (it unmounts on SIGTERM); never pkill.
    mount.terminate();
    if (!mount.waitForFinished(3000)) {
        mount.kill();
        mount.waitForFinished(3000);
    }
    return chromium;
}

bool isChromiumProgram(const QString &program, const QStringList &args)
{
    const AginoGpu::Root root = AginoGpu::Root::fromEnvironment();
    if (AginoGpu::isChromiumLike(root, program))
        return true;
    if (!AginoGpu::isAppImage(root, program))
        return false;
    // The AppImage runtime only treats --appimage-* as its own option when it comes first
    // (`--appimage-extract`, `--appimage-version`, ...): such a call is not an app launch.
    for (const QString &arg : args) {
        if (arg.startsWith(QLatin1String("--appimage-")))
            return false;
    }
    return appImageIsChromium(program);
}

} // namespace

int main(int argc, char **argv)
{
    int first = 1;
    bool forceChromium = false;
    if (first < argc && std::strcmp(argv[first], "--chromium") == 0) {
        forceChromium = true;
        ++first;
    }
    if (first < argc && (std::strcmp(argv[first], "--help") == 0 || std::strcmp(argv[first], "-h") == 0)) {
        usage(stdout);
        return 0;
    }
    if (first < argc && std::strcmp(argv[first], "--") == 0)
        ++first;
    if (first >= argc) {
        usage(stderr);
        return 2;
    }

    const QStringList environment = AginoGpu::readOffloadEnv(AginoGpu::Root::fromEnvironment());
    if (environment.isEmpty()) {
        std::fputs("agino-gpu-run: no discrete GPU available for offload; running normally\n", stderr);
    } else {
        for (const QString &entry : environment) {
            const qsizetype eq = entry.indexOf(QLatin1Char('='));
            if (eq <= 0)
                continue;
            const QByteArray key = entry.left(eq).toLocal8Bit();
            const QByteArray value = entry.mid(eq + 1).toLocal8Bit();
            setenv(key.constData(), value.constData(), 1);
        }
    }

    // design.md §5.4: Chromium and Electron need ANGLE on Vulkan to render on an NVIDIA GPU.
    QStringList args;
    for (int i = first + 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);
    QStringList extended = args;
    if (environment.contains(QStringLiteral("__NV_PRIME_RENDER_OFFLOAD=1"))) {
        const QString program = forceChromium ? QString() : canonicalProgram(argv[first]);
        if (forceChromium || (!program.isEmpty() && isChromiumProgram(program, args)))
            extended = AginoGpu::withChromiumOffloadArguments(args);
    }
    if (extended.size() != args.size()) {
        std::vector<QByteArray> storage;
        std::vector<char *> newArgv{argv[first]};
        for (const QString &arg : extended)
            storage.push_back(arg.toLocal8Bit());
        for (QByteArray &arg : storage)
            newArgv.push_back(arg.data());
        newArgv.push_back(nullptr);
        execvp(argv[first], newArgv.data());
    } else {
        execvp(argv[first], argv + first);
    }
    const int err = errno;
    std::fprintf(stderr, "agino-gpu-run: cannot execute '%s': %s\n", argv[first], std::strerror(err));
    return err == EACCES ? 126 : 127;
}
