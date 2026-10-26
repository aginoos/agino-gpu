# SPDX-License-Identifier: GPL-3.0-or-later
# Minimal Chrome DevTools Protocol client for the tests/manual scripts (standard library only, so it
# runs on a test machine as is). A browser that stops answering ends the script with a timeout error
# instead of hanging it.
import base64
import json
import os
import socket
import struct
import time
import urllib.request


class Cdp:
    def __init__(self, ws_url):
        host, port = ws_url.split("/")[2].split(":")
        path = "/" + ws_url.split("/", 3)[3]
        self.sock = socket.create_connection((host, int(port)), timeout=120)
        self.sock.send(f"GET {path} HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\n"
                       f"Connection: Upgrade\r\nSec-WebSocket-Key: {base64.b64encode(os.urandom(16)).decode()}\r\n"
                       "Sec-WebSocket-Version: 13\r\n\r\n".encode())
        buf = b""
        while b"\r\n\r\n" not in buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("DevTools closed the connection during the handshake")
            buf += chunk
        if not buf.startswith(b"HTTP/1.1 101"):
            raise ConnectionError(f"WebSocket handshake refused: {buf.splitlines()[0]!r}")
        self.data = buf.split(b"\r\n\r\n", 1)[1]
        self.next_id = 0
        self.events = []

    def _recv(self, n):
        while len(self.data) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("DevTools connection closed")
            self.data += chunk
        out, self.data = self.data[:n], self.data[n:]
        return out

    def _frame(self):
        head = self._recv(2)
        n = head[1] & 0x7f
        if n == 126:
            n = struct.unpack(">H", self._recv(2))[0]
        elif n == 127:
            n = struct.unpack(">Q", self._recv(8))[0]
        return json.loads(self._recv(n))

    def call(self, method, params=None):
        """Sends a command and returns its result; events received meanwhile go to self.events."""
        self.next_id += 1
        msg = json.dumps({"id": self.next_id, "method": method, "params": params or {}}).encode()
        n = len(msg)
        if n < 126:
            head = bytes([0x81, 0x80 | n])
        elif n < 65536:
            head = bytes([0x81, 0xFE]) + struct.pack(">H", n)
        else:
            head = bytes([0x81, 0xFF]) + struct.pack(">Q", n)
        mask = os.urandom(4)
        self.sock.send(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(msg)))
        while True:
            reply = self._frame()
            if reply.get("id") == self.next_id:
                if "error" in reply:
                    raise RuntimeError(f"{method}: {reply['error']}")
                return reply.get("result", {})
            self.events.append(reply)

    def evaluate(self, expression):
        """Evaluates a JS expression in the page and returns its value."""
        result = self.call("Runtime.evaluate", {"expression": expression, "returnByValue": True})
        return result.get("result", {}).get("value")


def browser(port):
    version = json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/json/version", timeout=10))
    return Cdp(version["webSocketDebuggerUrl"])


def page(port, url_part, wait=30):
    """Attaches to the page whose URL contains url_part. The page must be the program's start page: a tab
    opened later through /json/new stays in the background, where Chromium throttles timers and
    requestAnimationFrame, which stalls the tests."""
    deadline = time.time() + wait
    while True:
        pages = [t for t in json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/json/list", timeout=10))
                 if t["type"] == "page" and url_part in t["url"]]
        if pages:
            return Cdp(pages[0]["webSocketDebuggerUrl"])
        if time.time() > deadline:
            raise TimeoutError(f"no page with '{url_part}' in its URL on DevTools port {port}")
        time.sleep(1)
