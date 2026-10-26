# SPDX-License-Identifier: GPL-3.0-or-later
# Wait for the Khronos WebGL conformance harness (the program's start page, see chromium-webgl-cts) to
# finish and print its summary and the failing and timed-out pages.
# usage: python3 -I cdp-webgl-cts.py PORT TIMEOUT_S
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # cdp.py; -I leaves the script dir out
import cdp  # noqa: E402

port, timeout = int(sys.argv[1]), int(sys.argv[2])
page = cdp.page(port, "webgl-conformance-tests")
start = time.time()
text = ""
while time.time() - start < timeout:
    time.sleep(10)
    # The summary goes into a hidden <pre>: read textContent, innerText skips hidden elements.
    text = page.evaluate("(document.getElementById('testResultsAsText')||{}).textContent||''") or ""
    if "Test Summary" in text:
        break
if "Test Summary" not in text:
    status = page.evaluate("document.body.innerText.trim().split('\\n').slice(-1)[0] + ' | running: ' + "
                           "document.getElementById('test-iframe').src")
    print(f"TIMEOUT after {int(time.time() - start)} s; harness: {status}")
    sys.exit(1)
for line in text.splitlines():
    if line.startswith(("Unmasked RENDERER", "Test Summary", "PASSED", "NOT PASSED", "FAILED", "TIMED OUT",
                        "SKIPPED")) or "subtests ran" in line:
        print(line)
# The harness's JSON part is not valid JSON (no commas between result entries): cut the two lists.
for key in ("failures", "timeouts"):
    m = re.search(r'"' + key + r'": \[(.*?)\]', text, re.S)
    urls = sorted(u.split("/tests/")[-1] for u in re.findall(r'"([^"]+)"', m.group(1))) if m else ["(not found)"]
    print(f"{key}:", " ".join(urls) or "none")
