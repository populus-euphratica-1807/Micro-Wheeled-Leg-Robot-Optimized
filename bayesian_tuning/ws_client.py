#!/usr/bin/env python3
"""
WebSocket 客户端：用于向 ESP32 发送 PID 参数修改命令。
"""

import json
import websocket

class RobotWS:
    """封装对小车的 WebSocket 连接和调参命令"""
    
    def __init__(self, ip, port=81):
        self.url = f"ws://{ip}:{port}/"
        self.ws = None

    def connect(self):
        """建立连接"""
        self.ws = websocket.create_connection(self.url, timeout=3)
        print(f"[WS] 已连接 {self.url}")

    def set_pid(self, cmd, P=None, I=None, D=None):
        """
        发送 PID 参数修改命令。
        cmd: 'A'(pid_angle), 'B'(pid_gyro), 'H'(pid_lqr_u) 等
        """
        msg = {"mode": "pid", "cmd": cmd}
        if P is not None: msg["P"] = P
        if I is not None: msg["I"] = I
        if D is not None: msg["D"] = D
        self.ws.send(json.dumps(msg))
        # 读取并打印小车回执（可选）
        try:
            response = self.ws.recv()  # 小车会回传日志
            print(f"[WS] {response}")
        except:
            pass

    def set_height(self, height_mm):
        """修改目标高度（用于多工况测试场景）"""
        msg = {"mode": "basic", "height": height_mm}
        self.ws.send(json.dumps(msg))

    def close(self):
        if self.ws:
            self.ws.close()
            print("[WS] 连接已关闭")