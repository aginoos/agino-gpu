// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/root.h"

#include <QStringList>

namespace AginoGpu {

// design.md §5.4 (V-5, §5.4.1): on X11 with NVIDIA PRIME render offload Chromium's default
// ANGLE-GL backend fails and falls back to software rendering; ANGLE on Vulkan renders on the
// NVIDIA GPU. This is the only implementation of the switches: agino-gpu-run applies them, the
// DAM patch execs agino-gpu-run, and data/chromium.d/zz-agino-gpu hands over to it (--chromium).

// True when `dir` holds the files every Chromium/Electron runtime ships: icudtl.dat,
// resources.pak and v8_context_snapshot.bin.
bool hasChromiumRuntime(const Root &root, const QString &dir);

// `canonicalProgram` is an absolute, symlink-free path (without the Root prefix). True when the
// runtime files are in the program's directory or its parent (VS Code: bin/code). There is no
// name list; Debian's /usr/bin/chromium wrapper script is not detected (agino-gpu-run --chromium).
bool isChromiumLike(const Root &root, const QString &canonicalProgram);

// True for an AppImage: ELF magic and bytes 8..10 'A' 'I' 0x02 (type 2). Its contents are only
// visible when mounted, so the caller probes it (agino-gpu-run).
bool isAppImage(const Root &root, const QString &path);

// `args` are the arguments after the program. Only arguments before a literal "--" count.
// Unchanged when there is a --use-angle= switch or the last --disable-features= lists Vulkan,
// VulkanFromANGLE or DefaultANGLEVulkan. Otherwise --use-angle=vulkan is inserted at the front and
// the last --enable-features= is extended in place with the missing features of
// Vulkan,VulkanFromANGLE,DefaultANGLEVulkan (none: that switch is inserted at the front too).
QStringList withChromiumOffloadArguments(const QStringList &args);

} // namespace AginoGpu
