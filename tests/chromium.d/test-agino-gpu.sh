#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Sources data/chromium.d/agino-gpu the way Debian's /usr/bin/chromium wrapper does.
# usage: test-agino-gpu.sh <snippet>
set -u
snippet="$1"
fail=0
ANGLE="--use-angle=vulkan --enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan"

# check <name> <__NV_PRIME_RENDER_OFFLOAD value or "-"> <initial CHROMIUM_FLAGS> <expected>
check() {
    got=$(
        unset __NV_PRIME_RENDER_OFFLOAD
        [ "$2" != - ] && export __NV_PRIME_RENDER_OFFLOAD="$2"
        CHROMIUM_FLAGS="$3"
        . "$snippet"
        echo "$CHROMIUM_FLAGS"
    )
    if [ "$got" = "$4" ]; then
        echo "PASS $1"
    else
        echo "FAIL $1: got '$got', want '$4'"
        fail=1
    fi
}

check "nvidia offload, empty flags" 1 "" "$ANGLE"
check "nvidia offload, other flags kept" 1 "--foo" "--foo $ANGLE"
check "no offload variable" - "--foo" "--foo"
check "offload variable not 1" 0 "" ""
check "user --use-angle kept" 1 "--use-angle=gl" "--use-angle=gl"
exit $fail
