#!/usr/bin/env python3
"""
WebSocket 客户端模块，支持带退避的自动重连和脉冲扰动发送。
"""

import json
import time
import logging
import websocket

logger = logging.getLogger("WS")


class RobotWS:
    def __init__(self, ip, port=81, max_retries=3, backoff=2.0):
        self.ip = ip
        self.port = port
        self.url = f"ws://{ip}:{port}/"
        self.ws = None
        self.max_retries = max_retries
        self.backoff = backoff

    def connect(self):
        logger.info(f"连接 {self.url} ...")
        self.ws = websocket.create_connection(self.url, timeout=3)
        logger.info("WebSocket 已连接")

    def _ensure_connected(self):
        for attempt in range(1, self.max_retries + 1):
            if self.ws and self.ws.connected:
                return True
            logger.warning(f"尝试重连 ({attempt}/{self.max_retries})")
            try:
                self.connect()
                return True
            except Exception as e:
                logger.error(f"重连失败: {e}")
                time.sleep(self.backoff * (2 ** (attempt - 1)))
        logger.error("达到最大重连次数，放弃重连")
        return False

    def set_pid(self, cmd, P=None, I=None, D=None):
        if not self._ensure_connected():
            raise ConnectionError("WebSocket 未连接，无法发送 PID 指令")
        msg = {"mode": "pid", "cmd": cmd}
        if P is not None:
            msg["P"] = P
        if I is not None:
            msg["I"] = I
        if D is not None:
            msg["D"] = D
        try:
            self.ws.send(json.dumps(msg))
            _ = self.ws.recv()
        except Exception as e:
            logger.error(f"WebSocket 通信异常: {e}")
            self.ws = None
            raise ConnectionError("WebSocket 通信失败") from e

    def send_perturb(self, offset_deg, duration_ms=80):
        """发送脉冲扰动（偏移角度零点）"""
        if not self._ensure_connected():
            raise ConnectionError("WebSocket 未连接，无法发送扰动")
        msg = {
            "mode": "perturb",
            "offset": offset_deg,
            "duration": duration_ms
        }
        try:
            self.ws.send(json.dumps(msg))
            _ = self.ws.recv()
        except Exception as e:
            logger.error(f"扰动发送失败: {e}")
            self.ws = None
            raise ConnectionError("扰动发送失败") from e

    def close(self):
        if self.ws:
            self.ws.close()
            logger.info("WebSocket 已关闭")