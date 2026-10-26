# SPDX-License-Identifier: GPL-3.0-or-later
# Print Chromium's own GPU report (SystemInfo.getInfo) from a DevTools port. usage: python3 -I cdp-gpuinfo.py PORT
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # cdp.py; -I leaves the script dir out
import cdp  # noqa: E402

g = cdp.browser(int(sys.argv[1])).call("SystemInfo.getInfo")["gpu"]
a = g.get("auxAttributes", {})
fs = g.get("featureStatus", {})
print("renderer:", a.get("glRenderer"), "| impl:", a.get("glImplementationParts"))
print("features:", " ".join(f"{k}={fs[k]}" for k in ("gpu_compositing", "webgl", "webgl2", "webgpu", "rasterization",
                                                     "video_decode", "vulkan", "opengl") if k in fs))
