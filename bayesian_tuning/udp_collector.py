#!/usr/bin/env python3
"""
UDP 数据采集器：接收小车状态数据，支持按指定时长采集。
"""

import socket
import json
import time
import queue
import threading
import numpy as np

class UDPCollector:
    def __init__(self, port=12345, timeout=1.0):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("0.0.0.0", port))
        self.sock.settimeout(timeout)
        self.data_queue = queue.Queue()
        self._running = False

    def start(self):
        """启动后台接收线程"""
        self._running = True
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()
        print("[UDP] 接收已启动")

    def stop(self):
        """停止接收"""
        self._running = False
        self.sock.close()
        print("[UDP] 接收已停止")

    def _run(self):
        while self._running:
            try:
                data, _ = self.sock.recvfrom(4096)
                self.data_queue.put(json.loads(data.decode('utf-8')))
            except socket.timeout:
                continue
            except:
                break

    def collect(self, duration):
        """
        阻塞式采集 duration 秒数据，返回 (angle_error, time_stamps, failed)
        failed: True 表示小车失控或数据太少
        """
        records = []
        start = time.time()
        while time.time() - start < duration:
            try:
                records.append(self.data_queue.get(timeout=0.5))
            except queue.Empty:
                continue

        if len(records) < 10:
            return None, None, True

        times = np.array([r['t'] for r in records])
        t0 = times[0]
        time_stamps = (times - t0) / 1000.0   # 转秒
        angle_error = np.array([r['angle'] - r['angle_zp'] for r in records])
        # 检查是否失控
        failed = any(r.get('unctrl', 0) != 0 for r in records)
        return angle_error, time_stamps, failed