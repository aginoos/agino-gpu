# Test fixtures

Each directory is a miniature root file system (`sys/`, `proc/`, `usr/`) that the tests pass to
`AginoGpu::Root` as its prefix. **All fixtures here are synthetic**: hand-made from the kernel
layout of sysfs (relative symlinks exactly as the kernel creates them), not snapshots of a real
machine. Vendor/device ids are real-looking but the trees are not verified against hardware.
Replace or complement them with real snapshots as devices become available (design.md §9.1).

Symlink depth matters: `bus/pci/devices/<addr>` and `<dev>/driver` are relative and resolve
inside the fixture. Empty driver directories hold a `.gitkeep` so git keeps them.

| Fixture | Models |
|---|---|
| `hybrid-nvidia` | ASUS ROG GU605CW style H1: Intel iGPU 0000:00:02.0 (i915, boot_vga, card0, eDP-1 connected), NVIDIA 0000:01:00.0 (nvidia, card1, HDMI-A-1/DP-1 disconnected, runtime_status suspended) + HDMI audio 0000:01:00.1, root port 0000:00:01.0, AC (Mains, online) and BAT1; has a small `usr/share/hwdata/pci.ids` for the name lookup |
| `hybrid-nvidia-mux` | as above plus `asus-nb-wmi/gpu_mux_mode=1` (hybrid) and `dgpu_disable=0` (H2) |
| `mux-dedicated-active` | MUX in dedicated state: `gpu_mux_mode=0`, only the NVIDIA GPU visible (boot_vga, card1 with eDP-1 connected) |
| `hybrid-amd` | AMD iGPU 0000:c5:00.0 (amdgpu, boot_vga, card1, eDP-1) + AMD dGPU 0000:03:00.0 (amdgpu, card0) |
| `desktop-dgpu-display` | no eDP; Intel iGPU + NVIDIA, HDMI-A-1 connected on the NVIDIA card; no power supplies |
| `desktop-igpu-display` | no eDP; DP-1 connected on the Intel card, NVIDIA idle |
| `egpu` | laptop with eDP on the iGPU + NVIDIA 0000:05:00.0 behind two bridges whose `removable` says `removable` (Thunderbolt) |
| `single` | one Intel GPU with eDP-1 |
| `proc-users` | a copy of `hybrid-nvidia` sysfs plus `renderD128`/`renderD129` nodes and a `proc/` tree: pid 101 firefox (uid 1000, renderD129), 102 python3 (1000, /dev/nvidia0, nvidiactl, nvidia-uvm), 103 Xorg (uid 0, card1, nvidia-modeset), 104 bash (1001, card0), 105 mpv (1000, renderD128). fd links are dangling by design |

## Collecting a real fixture

On the machine (read-only, does not wake a suspended GPU):

    tests/collect-sysfs.sh /tmp/fixture-NAME
    tar -C /tmp/fixture-NAME -czf fixture-NAME.tar.gz .

Unpack into `tests/fixtures/<name>/`, add its entry to the table above (mark it as a real
snapshot with the machine and date), and add the expected classification to
`tests/lib/tst_detect.cpp`. `/proc` is not collected; extend a `proc/` tree by hand if needed.
