# Upstream reports (drafts)

Drafts written from the 2026-10-26 investigation (design.md §5.4.1). The user files them with their
own accounts (user decision 2026-10-26); once filed, put the links here and in design.md §5.4.1.
When an upstream fix ships, the ANGLE-Vulkan switches of agino-gpu-run can be revisited.

Status: not filed yet.

---

## 1. Chromium (issues.chromium.org, component Internals>GPU>ANGLE)

**Title:** Linux/X11: GPU process fails with "Invalid visual ID requested" under NVIDIA PRIME render offload when a compositing window manager runs

**Environment**

- Debian 13 (agino), Xorg 21.1.16 (xserver-xorg-core 2:21.1.16-1.3+deb13u4), kernel 7.2.9
- Intel Arrow Lake iGPU drives the X screen (modesetting + glamor); NVIDIA GeForce RTX 5080 Laptop
  (GB203M) as PRIME render offload provider `NVIDIA-G0`, NVIDIA driver 580.178.04, libglvnd 1.7.0
- KWin 6.3.6 (X11) compositing
- Chromium 156.0.8078.47 (Debian build; ANGLE 3b89d84e9d59f75675ac4e2bef58fc068a9f0577); the same
  happens with Google Chrome 156.0.8078.47, Microsoft Edge 156.0.4314.42 and Electron apps
  (Obsidian 1.14.5)

**Steps**

```
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/10_nvidia.json \
chromium --enable-logging=stderr
```

**Actual:** the GPU process logs, repeatedly,

```
ERR: Display.cpp:1505 (initialize): ANGLE Display::initialize error 12289: Invalid visual ID requested.
eglInitialize OpenGL failed with error EGL_NOT_INITIALIZED
```

and Chromium falls back to software: chrome://gpu shows GL implementation `(gl=disabled,angle=none)`,
gpu_compositing `disabled_software`, WebGL disabled. The discrete GPU is still woken up.

**Expected:** hardware acceleration on the NVIDIA GPU, as with other GLX applications (glxgears
reports `NVIDIA GeForce RTX 5080 Laptop GPU/PCIe/SSE2` with the same variables).

**Analysis**

1. `ui/base/x/x11_gl_egl_utility.cc:43-58` always passes `EGL_X11_VISUAL_ID_ANGLE`, chosen by
   `VisualManager::ChooseVisualForWindow(want_argb_visual=true)`. With a compositor
   (`_NET_WM_CM_S0` owned) that is the 32-bit ARGB visual (`ui/gfx/x/visual_manager.cc:85-98`).
   The candidate list comes from the GLX protocol request `GetVisualConfigs`
   (`ui/gfx/x/visual_picker_glx.cc:195-203`), i.e. from the X server's GLX, which here is Mesa on the
   Intel screen. Chromium's top-level window gets visual 0x6b9, depth 32.
2. ANGLE's GLX display lists the fbconfigs of the client libGL (glvnd → NVIDIA because of
   `__GLX_VENDOR_LIBRARY_NAME=nvidia`) and requires one whose `GLX_VISUAL_ID` equals the requested id;
   there is no fallback (`src/libANGLE/renderer/gl/glx/DisplayGLX.cpp:166-190`).
3. Under PRIME render offload the NVIDIA libGL exposes only 12 visuals, all depth 24
   (0x21, 0x22, 0x57a-0x57e, 0x619-0x61d), and no depth-32 visual at all, while the X server has 337
   visuals and Mesa exposes fbconfigs for 336 of them (`glxinfo -v` with and without the variables).
   So the ARGB visual chosen in step 1 never has a matching NVIDIA fbconfig.

**Confirmation:** with KWin compositing suspended, Chromium picks the opaque root visual 0x21 (depth 24),
which NVIDIA does expose, and the default GL backend works: `ANGLE (NVIDIA Corporation, NVIDIA GeForce
RTX 5080 Laptop GPU/PCIe/SSE2, OpenGL 4.5.0 NVIDIA 580.178.04)`, all features enabled. Resuming
compositing brings the error back.

**Workarounds observed:** `--use-angle=gl-egl` renders on NVIDIA but gpu_compositing stays
`disabled_software` and WebGL is `enabled_readback`. `--use-angle=vulkan
--enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan` works fully (Vulkan 1.4.312 on NVIDIA).

**Suggested fix:** when choosing the visual for an ANGLE GL/GLX display, only consider visuals for
which the client libGL has an fbconfig (`glXGetFBConfigs` + `GLX_VISUAL_ID`), and fall back to the
opaque visual when no ARGB visual qualifies, instead of asking the X server alone. Losing window
transparency is better than losing GPU acceleration. Alternatively, ANGLE could report a distinct,
actionable error so Chromium retries with the opaque visual.

Related user reports with the same message: NVIDIA forums threads 81600, 175472, 237948, 344123;
VirtualGL issue 146; crbug 1416297 (not visible to us).

---

## 2. NVIDIA (forums.developer.nvidia.com, Linux category)

**Title:** PRIME render offload: GLX exposes no depth-32 (ARGB) visuals, so GLX clients with ARGB windows cannot render (Chromium/Electron fail)

**System:** RTX 5080 Laptop (GB203M, 10de:2c59) + Intel Arrow Lake iGPU, driver 580.178.04,
Xorg 21.1.16 with modesetting/glamor on the iGPU, `NVIDIA-G0` offload provider, libglvnd 1.7.0,
compositing window manager (KWin X11). `nvidia-bug-report.sh` log available on request.

**Observation:** with `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia`, `glxinfo -v`
lists 12 visuals (0x21, 0x22, 0x57a-0x57e TrueColor, 0x619-0x61d DirectColor), all depth 24, and
106 fbconfigs. The X screen has 337 visuals including depth-32 ARGB ones, and Mesa on the same screen
exposes 336 of them. As a result any GLX client that renders into a window created with a depth-32
visual (what toolkits do under a compositor to get translucency) finds no NVIDIA fbconfig for that
visual. Chromium is the most visible victim: its GPU process asks ANGLE for the ARGB visual and fails
with "Invalid visual ID requested", then falls back to software rendering (see the Chromium report).

**Question:** is the missing depth-32 visual support under render offload a known limitation, and
could the driver expose fbconfigs for the screen's depth-32 TrueColor visuals in offload mode, as it
does on screens driven by the NVIDIA X driver? Vulkan and EGL clients are not affected.
