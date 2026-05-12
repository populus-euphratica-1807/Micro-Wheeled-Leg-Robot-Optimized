#!/usr/bin/env python3
"""
自动寻优主程序：贝叶斯优化 + WebSocket 调参 + UDP 采集
"""

import time
import numpy as np
from ws_client import RobotWS
from udp_collector import UDPCollector
from evaluator import compute_score
from mybayes_opt import create_optimizer

# ==================== 配置 ====================
ESP_IP = "192.168.1.11"
UDP_PORT = 12345
TEST_DURATION = 15.0
SETTLE_TIME = 3.0
N_INIT_POINTS = 5        # 初始随机探索点
N_ITER = 25              # 贝叶斯迭代次数（总 = init + iter）

# 搜索空间
SEARCH_SPACE = {
    "P_angle": (0.8, 3.5),
    "P_gyro":  (0.02, 0.15),
    "P_lqr_u": (0.6, 2.0),
    "I_lqr_u": (5.0, 30.0)
}

def evaluate_params(P_angle, P_gyro, P_lqr_u, I_lqr_u):
    """
    实际调参与评价函数，会被优化器调用。
    返回负数得分（因为优化器默认最大化，我们取负值使其最小化）。
    """
    # 下发参数
    ws.set_pid('A', P=float(P_angle))
    ws.set_pid('B', P=float(P_gyro))
    ws.set_pid('H', P=float(P_lqr_u), I=float(I_lqr_u))

    # 等待稳定
    time.sleep(SETTLE_TIME)

    # 采集数据
    angle_err, t_stamps, failed = udp.collect(TEST_DURATION)

    if failed or angle_err is None:
        score = 1e6
        print(f"  >> 失败，得分=1e6")
    else:
        score = compute_score(angle_err, t_stamps)
        print(f"  >> 得分={score:.3f}")

    # 返回负得分（优化器最大化该值，等效于最小化得分）
    return -score

def main():
    global ws, udp

    # 1. 启动 UDP
    udp = UDPCollector(UDP_PORT)
    udp.start()

    # 2. 连接 WebSocket
    ws = RobotWS(ESP_IP)
    ws.connect()
    time.sleep(1)

    # 3. 定义目标函数（供优化器调用）
    def target_function(P_angle, P_gyro, P_lqr_u, I_lqr_u):
        return evaluate_params(P_angle, P_gyro, P_lqr_u, I_lqr_u)

    # 4. 创建优化器（同时绑定目标函数）
    optimizer = create_optimizer(target_function, SEARCH_SPACE, random_state=42)

    # 5. 开始寻优
    print("\n开始自动寻优...")
    optimizer.maximize(
        init_points=N_INIT_POINTS,
        n_iter=N_ITER
    )

    # 6. 输出结果
    best = optimizer.max
    print("\n" + "="*50)
    print("寻优完成！")
    print(f"最优参数: A.P={best['params']['P_angle']:.4f}, "
          f"B.P={best['params']['P_gyro']:.4f}, "
          f"H.P={best['params']['P_lqr_u']:.4f}, "
          f"H.I={best['params']['I_lqr_u']:.2f}")
    print(f"最优得分 (原始): {-best['target']:.4f}")

    # 7. 清理
    udp.stop()
    ws.close()

if __name__ == "__main__":
    main()