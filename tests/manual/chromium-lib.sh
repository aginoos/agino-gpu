# SPDX-License-Identifier: GPL-3.0-or-later
# Sourced by the tests/manual/chromium-* scripts: start a Chromium-based program (browser or Electron
# app) in the current X session with a DevTools port, find its processes, close it. Uses $port.

# The runtime PM status file of the NVIDIA display controller, empty without one.
dgpu_pm() {
    for f in /sys/bus/pci/devices/*; do
        [ "$(cat "$f/vendor")" = 0x10de ] && [ "$(cut -c1-4 "$f/class")" = 0x03 ] && echo "$f/power/runtime_status"
    done | head -1
}

# start_program LOG COMMAND...: runs COMMAND --remote-debugging-port=$port in the background with its
# output in LOG and waits for the DevTools endpoint.
start_program() {
    _log=$1; shift
    setsid "$@" --remote-debugging-port="$port" >"$_log" 2>&1 </dev/null &
    _i=0
    until curl -s -o /dev/null "http://127.0.0.1:$port/json/version"; do
        _i=$((_i + 1)); [ $_i -gt 40 ] && { echo "no DevTools endpoint on port $port" >&2; return 1; }
        sleep 0.5
    done
}

# The browser/main processes carry the DevTools switch as an argument of its own (not e.g. inside a
# shell's -c script, which pgrep -f also matches) and no --type=.
main_pids() {
    for _p in $(pgrep -f -- "--remote-debugging-port=$port"); do
        _args=$(tr '\0' '\n' <"/proc/$_p/cmdline" 2>/dev/null)
        echo "$_args" | grep -qx -- "--remote-debugging-port=$port" || continue
        echo "$_args" | grep -q -- '--type=' || echo "$_p"
    done
}

# The GPU process is a child of a main process or, in Chrome, of the zygote the main process started.
gpu_pids() {
    for _m in $(main_pids); do
        for _p in $(pgrep -P "$_m") $(for _c in $(pgrep -P "$_m"); do pgrep -P "$_c"; done); do
            tr '\0' '\n' <"/proc/$_p/cmdline" 2>/dev/null | grep -q -- '--type=gpu-process' && echo "$_p"
        done
    done
}

any_alive() {
    for _a in "$@"; do [ -d "/proc/$_a" ] && return 0; done
    return 1
}

# Ends exactly the main processes (never pkill -f) and waits up to 10 s for them to exit. A main process
# whose parent is also one is left alone: in an AppImage that is the runtime's FUSE child (same command
# line), which unmounts once the app exits; killing it first makes the exiting app die of SIGBUS.
close_program() {
    _mains=$(main_pids)
    for _m in $_mains; do
        _pp=$(ps -o ppid= -p "$_m" | tr -d ' ')
        case " $(echo $_mains) " in *" $_pp "*) ;; *) kill "$_m" 2>/dev/null ;; esac
    done
    _i=0
    # shellcheck disable=SC2086 # a list of PIDs
    while [ $_i -lt 20 ] && any_alive $_mains; do sleep 0.5; _i=$((_i + 1)); done
}

# Seconds until the dGPU is back in runtime suspend (at most 60), printed as "STATUS after Ns".
dgpu_settle() {
    _pm=$(dgpu_pm); [ -n "$_pm" ] || return 0
    _t=0
    while [ "$(cat "$_pm")" != suspended ] && [ $_t -lt 60 ]; do sleep 1; _t=$((_t + 1)); done
    echo "dgpu after close: $(cat "$_pm") after ${_t}s"
}
