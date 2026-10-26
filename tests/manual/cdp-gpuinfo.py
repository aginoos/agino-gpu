# SPDX-License-Identifier: GPL-3.0-or-later
# Print Chromium's own GPU report (SystemInfo.getInfo) from a DevTools port. usage: python3 -I cdp-gpuinfo.py PORT
import json, urllib.request, socket, base64, os, struct, sys
port = int(sys.argv[1])
ws = json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/json/version"))["webSocketDebuggerUrl"]
host, p = ws.split("/")[2].split(":"); path = "/" + ws.split("/", 3)[3]
s = socket.create_connection((host, int(p)))
s.send(f"GET {path} HTTP/1.1\r\nHost: {host}:{p}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
       f"Sec-WebSocket-Key: {base64.b64encode(os.urandom(16)).decode()}\r\nSec-WebSocket-Version: 13\r\n\r\n".encode())
buf = b""
while b"\r\n\r\n" not in buf: buf += s.recv(4096)
data = buf.split(b"\r\n\r\n", 1)[1]
msg = json.dumps({"id": 1, "method": "SystemInfo.getInfo"}).encode(); mask = os.urandom(4)
hdr = bytes([0x81]) + (bytes([0x80 | len(msg)]) if len(msg) < 126 else bytes([0xFE]) + struct.pack(">H", len(msg)))
s.send(hdr + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(msg)))
def recvn(n):
    global data
    while len(data) < n: data += s.recv(65536)
    r, data = data[:n], data[n:]; return r
h = recvn(2); L = h[1] & 0x7f
L = struct.unpack(">H", recvn(2))[0] if L == 126 else struct.unpack(">Q", recvn(8))[0] if L == 127 else L
g = json.loads(recvn(L))["result"]["gpu"]; a = g.get("auxAttributes", {}); fs = g.get("featureStatus", {})
print("renderer:", a.get("glRenderer"), "| impl:", a.get("glImplementationParts"))
print("features:", " ".join(f"{k}={fs[k]}" for k in ("gpu_compositing", "webgl", "webgl2", "webgpu", "rasterization", "video_decode", "vulkan", "opengl") if k in fs))
