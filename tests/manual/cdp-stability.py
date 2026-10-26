# SPDX-License-Identifier: GPL-3.0-or-later
# Sample chromium-stability.html (the program's start page, see chromium-stability) for DURATION_S and
# print the video position, dropped frames, WebGL frame count and GPU-process PIDs every INTERVAL_S,
# then the video decoder Chromium chose (Media domain) and a summary.
# usage: python3 -I cdp-stability.py PORT MAIN_PIDS DURATION_S INTERVAL_S   (MAIN_PIDS comma-separated)
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # cdp.py; -I leaves the script dir out
import cdp  # noqa: E402

port, mains = int(sys.argv[1]), [int(p) for p in sys.argv[2].split(",") if p]
duration, interval = int(sys.argv[3]), int(sys.argv[4])


def gpu_pids():
    children = {}
    for d in os.listdir("/proc"):
        if not d.isdigit():
            continue
        try:
            stat = open(f"/proc/{d}/stat").read()
            children.setdefault(int(stat[stat.rindex(")") + 2:].split()[1]), []).append(int(d))
        except (OSError, ValueError):
            pass
    found, todo = [], list(mains)
    while todo:
        for child in children.get(todo.pop(), []):
            todo.append(child)
            try:
                if b"--type=gpu-process" in open(f"/proc/{child}/cmdline", "rb").read():
                    found.append(child)
            except OSError:
                pass
    return sorted(found)


page = cdp.page(port, "chromium-stability.html")
page.call("Media.enable")
expr = ("JSON.stringify((()=>{const v=document.getElementById('v');const q=v.getVideoPlaybackQuality();"
        "return {t:v.currentTime,err:v.error&&v.error.code,dropped:q.droppedVideoFrames,total:q.totalVideoFrames,"
        "frames:window.__frames};})())")
start = time.time()
seen, samples = set(), []
while True:
    v = json.loads(page.evaluate(expr))
    g = gpu_pids()
    seen.update(g)
    samples.append((time.time() - start, v))
    print(f"{int(time.time() - start):4d}s video_t={v['t']:.1f} dropped={v['dropped']}/{v['total']} "
          f"webgl_frames={v['frames']} gpu={g} err={v['err']}", flush=True)
    if time.time() - start >= duration:
        break
    time.sleep(interval)
props = {}
for e in page.events:
    if e.get("method") == "Media.playerPropertiesChanged":
        for pr in e["params"]["properties"]:
            props[pr["name"]] = pr["value"]
elapsed = samples[-1][0] - samples[0][0]
fps = (samples[-1][1]["frames"] - samples[0][1]["frames"]) / elapsed if elapsed else 0
print("decoder:", props.get("kVideoDecoderName"), "platform:", props.get("kIsPlatformVideoDecoder"),
      "| renderer:", props.get("kRendererName"))
print(f"summary: gpu processes seen={sorted(seen)} (restarts={max(len(seen) - 1, 0)}) "
      f"dropped={samples[-1][1]['dropped']}/{samples[-1][1]['total']} webgl_fps={fps:.1f} "
      f"video_error={samples[-1][1]['err']}")
