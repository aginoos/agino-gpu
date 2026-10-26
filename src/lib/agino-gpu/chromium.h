// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/root.h"

#include <QStringList>

namespace AginoGpu {

// design.md §5.4 (V-5): on X11 with NVIDIA PRIME render offload Chromium's default ANGLE-GL
// backend fails and falls back to software rendering; ANGLE on Vulkan renders on the NVIDIA GPU.
// The same rules are implemented by data/chromium.d/agino-gpu (Debian's wrapper) and by the DAM patch.

// `canonicalProgram` is an absolute, symlink-free path (without the Root prefix).
// True for a known browser name (chromium, google-chrome*, microsoft-edge*, brave*, vivaldi*,
// opera, electron, electron<N>) or an Electron app (resources/app.asar or resources/app/package.json
// next to the program or one directory above it).
bool isChromiumLike(const Root &root, const QString &canonicalProgram);

// {"--use-angle=vulkan", "--enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan"}
QStringList chromiumOffloadArguments();

// `args` are the arguments after the program. The two switches are prepended (so the user's own
// switches win) when `environment` ("KEY=VALUE") has __NV_PRIME_RENDER_OFFLOAD=1, the program
// is Chromium-like and `args` has no --use-angle= switch; otherwise `args` is returned unchanged.
QStringList withChromiumOffloadArguments(const Root &root, const QString &canonicalProgram,
                                         const QStringList &args, const QStringList &environment);

} // namespace AginoGpu
