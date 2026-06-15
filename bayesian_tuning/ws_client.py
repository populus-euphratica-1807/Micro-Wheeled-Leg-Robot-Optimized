#!/usr/bin/env python3
"""
WebSocket 客户端
"""

import json
import websocket

class RobotWS:
    def __init__(self, ip, port=81):
        self.url = f"ws://{ip}:{port}/"
        self.ws = None

    def connect(self):
        print(f"[WS] 连接 {self.url} ...", flush=True)
        self.ws = websocket.create_connection(self.url, timeout=3)
        print(f"[WS] 已连接", flush=True)

    def set_pid(self, cmd, P=None, I=None, D=None):
        msg = {"mode": "pid", "cmd": cmd}
        if P is not None: msg["P"] = P
        if I is not None: msg["I"] = I
        if D is not None: msg["D"] = D
        self.ws.send(json.dumps(msg))
        try:
            resp = self.ws.recv()
            print(f"[WS] {resp}", flush=True)
        except:
            pass

    def close(self):
        if self.ws:
            self.ws.close()
            print("[WS] 连接已关闭", flush=True)