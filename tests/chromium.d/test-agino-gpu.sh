#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Sources data/chromium.d/zz-agino-gpu the way Debian's /usr/bin/chromium wrapper does, with
# /usr/bin/agino-gpu-run replaced by a fake that records its argv and AGINO_GPU_CHROMIUM_D.
# usage: test-agino-gpu.sh <snippet>
set -u
snippet="$1"
fail=0
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

cat >"$tmp/fake-run" <<'FAKE'
#!/bin/sh
{ echo "marker=${AGINO_GPU_CHROMIUM_D:-}"; for a in "$@"; do echo "arg=$a"; done; } >"$FAKE_OUT"
FAKE
chmod +x "$tmp/fake-run"
sed "s|/usr/bin/agino-gpu-run|$tmp/fake-run|g" "$snippet" >"$tmp/snippet"
sed "s|/usr/bin/agino-gpu-run|$tmp/missing-run|g" "$snippet" >"$tmp/snippet-missing"

# run <snippet> <offload value or -> <marker or -> <CHROMIUM_FLAGS> [ARGS...]
# Prints the fake's record, then "after" when the snippet fell through (no exec).
run() {
    file="$1" value="$2" marker="$3" flags="$4"
    shift 4
    rm -f "$tmp/out"
    (
        unset __NV_PRIME_RENDER_OFFLOAD AGINO_GPU_CHROMIUM_D
        [ "$value" != - ] && export __NV_PRIME_RENDER_OFFLOAD="$value"
        [ "$marker" != - ] && export AGINO_GPU_CHROMIUM_D="$marker"
        export FAKE_OUT="$tmp/out"
        CHROMIUM_FLAGS="$flags"
        set -- "$@"
        # $0 is the wrapper's name when it sources the file with "."
        eval '. "$file"'
        echo "after marker=${AGINO_GPU_CHROMIUM_D:-}" >"$tmp/after"
    ) 2>/dev/null
    [ -f "$tmp/out" ] && cat "$tmp/out"
    [ -f "$tmp/after" ] && { cat "$tmp/after"; rm -f "$tmp/after"; }
    return 0
}

check() {
    name="$1" got="$2" want="$3"
    if [ "$got" = "$want" ]; then
        echo "PASS $name"
    else
        echo "FAIL $name: got '$got', want '$want'"
        fail=1
    fi
}

nl='
'
# Under "sh test.sh" the subshell's $0 is the test script's path.
check "no offload variable" "$(run "$tmp/snippet" - - "--foo" x)" "after marker="
check "offload variable not 1" "$(run "$tmp/snippet" 0 - "" x)" "after marker="
# exec replaces the subshell, so "after" is absent and the record shows the call.
check "offload hands over" "$(run "$tmp/snippet" 1 - "--foo --bar" url1 url2)" \
    "marker=1${nl}arg=--chromium${nl}arg=--${nl}arg=$0${nl}arg=--foo${nl}arg=--bar${nl}arg=url1${nl}arg=url2"
check "offload, empty flags" "$(run "$tmp/snippet" 1 - "" --incognito)" \
    "marker=1${nl}arg=--chromium${nl}arg=--${nl}arg=$0${nl}arg=--incognito"
check "marker set: no exec, marker unset" "$(run "$tmp/snippet" 1 1 "--foo" x)" "after marker="
check "agino-gpu-run missing" "$(run "$tmp/snippet-missing" 1 - "--foo" x)" "after marker="
exit $fail
