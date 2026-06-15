#!/usr/bin/env python3
"""
UDP 数据采集器，线程安全获取最新记录。
"""

import socket
import json
import time
import queue
import threading
import logging
import numpy as np

logger = logging.getLogger("UDP")


class UDPCollector:
    def __init__(self, port=12345, timeout=1.0):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("0.0.0.0", port))
        self.sock.settimeout(timeout)

        self.data_queue = queue.Queue()
        self._running = False
        self._lock = threading.Lock()
        self._latest_record = None
        self.last_records = []

    @property
    def latest_record(self):
        with self._lock:
            return self._latest_record

    def start(self):
        logger.info("启动 UDP 接收线程")
        self._running = True
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def stop(self):
        logger.info("停止 UDP 接收")
        self._running = False
        self.sock.close()

    def _run(self):
        while self._running:
            try:
                data, _ = self.sock.recvfrom(4096)
                record = json.loads(data.decode('utf-8'))
                self.data_queue.put(record)
                with self._lock:
                    self._latest_record = record
            except socket.timeout:
                continue
            except Exception as e:
                if self._running:
                    logger.error(f"UDP 接收异常: {e}")
                break

    def collect(self, duration, angle_threshold=45.0, gyro_threshold=100.0, consecutive_checks=10):
        # 清空队列中已有的历史数据，确保只采集调用之后的数据
        while not self.data_queue.empty():
            try:
                self.data_queue.get_nowait()
            except queue.Empty:
                break

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
                        logger.warning(f"连续 {consecutive_checks} 帧摔倒条件触发")
                        self.last_records = records
                        return None, None, True
                else:
                    fail_count = 0
            except queue.Empty:
                continue

        self.last_records = records
        if len(records) < 10:
            logger.warning("采集数据过少")
            return None, None, True

        times = np.array([r['t'] for r in records])
        t0 = times[0]
        time_stamps = (times - t0) / 1000.0
        angle_error = np.array([r['angle'] - r['angle_zp'] for r in records])
        gyro_data = np.array([r.get('gyro', 0) for r in records])

        if np.std(angle_error) < 0.02 and np.std(gyro_data) < 0.5:
            logger.warning("数据无有效波动，判定失败")
            return None, None, True

        return angle_error, time_stamps, False