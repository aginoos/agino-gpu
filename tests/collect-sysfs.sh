#!/bin/bash
# Copies the sysfs attributes agino-gpu reads into a fixture tree, keeping the real relative
# symlinks (bus/pci/devices, class/drm, driver, device). Read-only and does not wake a
# suspended GPU: no config space reads; connector "status" is a cached value (V-12).
# Usage: collect-sysfs.sh OUTDIR   (then: tar -C OUTDIR -czf fixture.tar.gz .)
set -eu
out=$1
[ -e "$out" ] && { echo "$out exists" >&2; exit 1; }
mkdir -p "$out"

copy_attr() { # $1 = absolute sysfs file
    [ -r "$1" ] || return 0
    mkdir -p "$out$(dirname "$1")"
    cat "$1" > "$out$1" 2>/dev/null || rm -f "$out$1"
}
copy_link() { # $1 = absolute symlink path; recreate with the same relative target
    [ -L "$1" ] || return 0
    mkdir -p "$out$(dirname "$1")"
    ln -s "$(readlink "$1")" "$out$1"
}

pci_dev() { # $1 = /sys/bus/pci/devices/<addr>
    local real
    real=$(readlink -f "$1")
    for a in vendor device class boot_vga subsystem_vendor subsystem_device removable \
             d3cold_allowed power/control power/runtime_status label; do
        copy_attr "$real/$a"
    done
    copy_link "$real/driver"
    copy_link "$1"
    [ -d "$out$real" ] || mkdir -p "$out$real"
}

for d in /sys/bus/pci/devices/*; do
    class=$(cat "$d/class")
    case $class in 0x03*|0x0403*) ;; *) continue ;; esac
    pci_dev "$d"
    # the bridges above it (for Thunderbolt / root port checks)
    p=$(dirname "$(readlink -f "$d")")
    while [ -e "$p/vendor" ]; do
        a=$(basename "$p"); pci_dev "/sys/bus/pci/devices/$a"; p=$(dirname "$p")
    done
done
# driver directories, so the driver symlinks resolve inside the fixture
for l in $(find "$out/sys/devices" -name driver -type l); do
    mkdir -p "$out/sys/bus/pci/drivers/$(basename "$(readlink "$l")")"
done

for e in /sys/class/drm/*; do
    case $(basename "$e") in version) continue ;; esac
    real=$(readlink -f "$e")
    copy_link "$e"
    mkdir -p "$out$real"
    copy_link "$real/device"
    if [ -e "$real/enabled" ]; then # connector
        copy_attr "$real/enabled"; copy_attr "$real/dpms"; copy_attr "$real/status"
    fi
done

for f in gpu_mux_mode dgpu_disable egpu_enable; do
    copy_attr /sys/devices/platform/asus-nb-wmi/$f
done
for s in /sys/class/power_supply/*; do
    real=$(readlink -f "$s"); copy_link "$s"
    copy_attr "$real/type"; copy_attr "$real/online"; copy_attr "$real/scope"
done
copy_attr /sys/module/nvidia/version
copy_attr /sys/module/nvidia/taint
for v in /sys/firmware/efi/efivars/SecureBoot-*; do
    [ -e "$v" ] || continue
    mkdir -p "$out$(dirname "$v")"; cat "$v" > "$out$v"
done
echo "collected into $out"
