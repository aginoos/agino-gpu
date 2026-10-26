#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Sources data/chromium.d/agino-gpu the way Debian's /usr/bin/chromium wrapper does.
# usage: test-agino-gpu.sh <snippet>
set -u
snippet="$1"
fail=0
ANGLE="--use-angle=vulkan --enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan"

# check <name> <__NV_PRIME_RENDER_OFFLOAD value or "-"> <initial CHROMIUM_FLAGS> <expected> [ARGS...]
# ARGS become the positional parameters the snippet sees (the wrapper's command line).
check() {
    name="$1" value="$2" flags="$3" want="$4"
    shift 4
    got=$(
        unset __NV_PRIME_RENDER_OFFLOAD
        [ "$value" != - ] && export __NV_PRIME_RENDER_OFFLOAD="$value"
        CHROMIUM_FLAGS="$flags"
        . "$snippet"
        echo "$CHROMIUM_FLAGS"
    )
    if [ "$got" = "$want" ]; then
        echo "PASS $name"
    else
        echo "FAIL $name: got '$got', want '$want'"
        fail=1
    fi
}

check "nvidia offload, empty flags" 1 "" "$ANGLE"
check "nvidia offload, other flags kept" 1 "--foo" "--foo $ANGLE"
check "no offload variable" - "--foo" "--foo"
check "offload variable not 1" 0 "" ""
check "user --use-angle kept" 1 "--use-angle=gl" "--use-angle=gl"
check "--use-angle on the command line" 1 "--foo" "--foo" --use-angle=vulkan https://example.org
check "unrelated command line" 1 "" "$ANGLE" --incognito
exit $fail
