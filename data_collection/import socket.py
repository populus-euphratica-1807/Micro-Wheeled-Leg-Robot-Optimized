#!/usr/bin/env python3
"""
平衡小车 UDP 数据采集脚本（优化版）
接收 ESP32 通过 WiFi 发来的 JSON 状态包，实时存入 CSV 文件。
与固件字段严格对齐，自动补全缺失值，具备健壮的错误处理。
"""

import socket
import json
import csv
import time
import os
import signal
import sys
from datetime import datetime

# ==================== 配置 ====================
UDP_IP = "0.0.0.0"                  # 监听所有网络接口
UDP_PORT = 12345                    # 与小车固件 UDP_TARGET_PORT 一致
CSV_PREFIX = "robot_data"           # 文件名前缀

# 与 ESP32 发送的 JSON 字段完全一致（顺序无关，但此处作为 CSV 表头）
HEADER = [
    "t", "angle", "gyro", "speed", "dist", "LQR_u", "yawout", "yaw",
    "joyx", "joyy", "joyy_f", "height", "angle_zp", "dist_zp",
    "leg_add", "bat", "motor1_t", "motor2_t", "unctrl", "jump"
]

# 超时（秒），用于定期唤醒以检查退出标志
SOCK_TIMEOUT = 1.0
# 每收到这么多个包打印一次状态
PRINT_INTERVAL = 50

# ==================== 全局退出标志 ====================
running = True

def signal_handler(sig, frame):
    """处理 Ctrl+C 信号，优雅退出"""
    global running
    print("\n\n正在停止采集...")
    running = False

signal.signal(signal.SIGINT, signal_handler)

# ==================== 主程序 ====================
def main():
    # 生成带时间戳的 CSV 文件名
    timestamp_str = datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_filename = f"{CSV_PREFIX}_{timestamp_str}.csv"

    # 创建 UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((UDP_IP, UDP_PORT))
    sock.settimeout(SOCK_TIMEOUT)
    print(f"监听 UDP 端口 {UDP_PORT} ...")

    # 打开 CSV 文件并写入表头
    with open(csv_filename, mode='w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(HEADER)

        packet_count = 0
        start_time = time.time()
        last_print = start_time
        total_bytes = 0

        print(f"开始记录到 {csv_filename} ... 按 Ctrl+C 停止。\n")

        while running:
            try:
                data, addr = sock.recvfrom(4096)
            except socket.timeout:
                continue    # 仅用于检查 running 标志，无数据时循环
            except OSError as e:
                print(f"Socket 错误: {e}")
                break

            # 解析 JSON
            try:
                payload = json.loads(data.decode('utf-8'))
            except (json.JSONDecodeError, UnicodeDecodeError):
                print("JSON 解析失败，跳过数据包")
                continue

            # 按表头顺序提取数据，缺失字段填充 0.0（避免 CSV 中出现 "None" 字符串）
            row = []
            for field in HEADER:
                value = payload.get(field)
                if value is None:
                    value = 0.0   # 或者可以用 float('nan') 保留空字段
                row.append(value)

            writer.writerow(row)
            f.flush()   # 确保数据实时写入磁盘

            packet_count += 1
            total_bytes += len(data)

            # 定期打印状态
            now = time.time()
            if packet_count % PRINT_INTERVAL == 0:
                elapsed = now - start_time
                # 从当前包中提取关键字段用于展示
                t = payload.get('t', 0)
                angle = payload.get('angle', 0.0)
                speed = payload.get('speed', 0.0)
                bat = payload.get('bat', 0.0)
                print(f"已收 {packet_count} 包 | "
                      f"t={t} ms | "
                      f"angle={angle:.2f}° | "
                      f"speed={speed:.2f} rad/s | "
                      f"bat={bat:.2f} V | "
                      f"运行 {elapsed:.1f} s")

        # 循环结束（running 变为 False 或 socket 错误）
        elapsed = time.time() - start_time
        print("\n采集结束。")
        print(f"总包数: {packet_count}")
        print(f"总字节: {total_bytes}")
        print(f"持续时间: {elapsed:.1f} s")
        if packet_count > 0:
            print(f"平均速率: {packet_count / elapsed:.2f} pkt/s")
        print(f"数据保存至: {csv_filename}")

    sock.close()

if __name__ == "__main__":
    main()



    