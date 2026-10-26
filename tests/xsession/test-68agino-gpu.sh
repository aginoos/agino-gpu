#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Sources data/xsession/68agino-gpu the way /etc/X11/Xsession does, against a fake root.
# usage: test-68agino-gpu.sh <snippet>
set -u
snippet="$1"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0

# check <name> <offload.env contents or "-" for none> <preset decoding value or "-"> <expected>
check() {
    rm -rf "$tmp/run"
    if [ "$2" != - ]; then
        mkdir -p "$tmp/run/agino-gpu"
        printf '%s\n' "$2" > "$tmp/run/agino-gpu/offload.env"
    fi
    got=$(
        unset QT_FFMPEG_DECODING_HW_DEVICE_TYPES QT_FFMPEG_ENCODING_HW_DEVICE_TYPES
        [ "$3" != - ] && export QT_FFMPEG_DECODING_HW_DEVICE_TYPES="$3"
        AGINO_GPU_ROOT="$tmp"
        . "$snippet"
        echo "${QT_FFMPEG_DECODING_HW_DEVICE_TYPES-unset}/${QT_FFMPEG_ENCODING_HW_DEVICE_TYPES-unset}"
    )
    if [ "$got" = "$4" ]; then
        echo "PASS $1"
    else
        echo "FAIL $1: got $got, want $4"
        fail=1
    fi
}

check "nvidia hybrid" "__NV_PRIME_RENDER_OFFLOAD=1" - "vaapi/vaapi"
check "no offload.env (dedicated, integrated, single GPU)" - - "unset/unset"
check "mesa discrete GPU" "DRI_PRIME=pci-0000_03_00_0" - "unset/unset"
check "user value kept" "__NV_PRIME_RENDER_OFFLOAD=1" "cuda" "cuda/vaapi"
exit $fail
