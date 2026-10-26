// SPDX-License-Identifier: GPL-3.0-or-later
// agino-gpu-run [--] COMMAND [ARGS...]: run a command on the discrete GPU (design.md §5.1).

#include "agino-gpu/offloadenv.h"
#include "agino-gpu/root.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

void usage(FILE *out)
{
    std::fputs("Usage: agino-gpu-run [--] COMMAND [ARGS...]\n"
               "Run COMMAND on the discrete GPU (PRIME render offload).\n"
               "When no discrete GPU is available, COMMAND runs unchanged.\n",
               out);
}

} // namespace

int main(int argc, char **argv)
{
    int first = 1;
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

    execvp(argv[first], argv + first);
    const int err = errno;
    std::fprintf(stderr, "agino-gpu-run: cannot execute '%s': %s\n", argv[first], std::strerror(err));
    return err == EACCES ? 126 : 127;
}
