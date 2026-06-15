#!/usr/bin/env python3
"""
UDP 数据采集器（修复版）
- 连续10帧角度超45°且角速度超100°/s才判倒
- 实时更新 latest_record
"""

import socket
import json
import time
import queue
import threading
import numpy as np

class UDPCollector:
    def __init__(self, port=12345, timeout=1.0):
        print("[UDP] 初始化...", flush=True)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("0.0.0.0", port))
        self.sock.settimeout(timeout)
        self.data_queue = queue.Queue()
        self._running = False
        self.latest_record = None
        self.last_records = []
        print("[UDP] 初始化完成", flush=True)

    def start(self):
        print("[UDP] 启动接收线程...", flush=True)
        self._running = True
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()
        print("[UDP] 接收已启动", flush=True)

    def stop(self):
        print("[UDP] 停止接收...", flush=True)
        self._running = False
        self.sock.close()
        print("[UDP] 接收已停止", flush=True)

    def _run(self):
        while self._running:
            try:
                data, _ = self.sock.recvfrom(4096)
                record = json.loads(data.decode('utf-8'))
                self.data_queue.put(record)
                self.latest_record = record
            except socket.timeout:
                continue
            except Exception as e:
                if self._running:
                    print(f"[UDP] 接收异常: {e}", flush=True)
                break

    def collect(self, duration, angle_threshold=45.0, gyro_threshold=100.0, consecutive_checks=10):
        """
        采集 duration 秒数据。
        只有同时满足：|angle - angle_zp| > angle_threshold 且 |gyro| > gyro_threshold 
        且连续 consecutive_checks 帧，才判定摔倒。
        """
        records = []
        start = time.time()
        fail_count = 0
        while time.time() - start < duration:
            try:
                record = self.data_queue.get(timeout=0.5)
                records.append(record)
                angle = record.get('angle', 0)
                angle_zp = record.get('angle_zp', 0)
                gyro = record.get('gyro', 0)

                if abs(angle - angle_zp) > angle_threshold and abs(gyro) > gyro_threshold:
                    fail_count += 1
                    if fail_count >= consecutive_checks:
                        print(f"   >>> 连续 {consecutive_checks} 帧同时超角度({angle_threshold}°)与角速度({gyro_threshold}°/s)，判定摔倒", flush=True)
                        self.last_records = records
                        return None, None, True
                else:
                    fail_count = 0
            except queue.Empty:
                continue

        self.last_records = records
        if len(records) < 10:
            print("   >>> 采集数据过少", flush=True)
            return None, None, True

        times = np.array([r['t'] for r in records])
        t0 = times[0]
        time_stamps = (times - t0) / 1000.0
        angle_error = np.array([r['angle'] - r['angle_zp'] for r in records])
        gyro_data = np.array([r.get('gyro', 0) for r in records])

        # 检查数据是否“静止”（机器人未启动）
        if np.std(angle_error) < 0.02 and np.std(gyro_data) < 0.5:
            print("   >>> 数据无有效波动，判定失败（机器人可能已停止）", flush=True)
            return None, None, True

        return angle_error, time_stamps, False