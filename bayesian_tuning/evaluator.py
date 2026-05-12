#!/usr/bin/env python3
"""
评价函数：从角度误差数据计算控制性能得分。
"""

import numpy as np

def compute_score(angle_error, time_stamps, overshoot_threshold=1.5):
    """
    参数:
        angle_error: 角度误差序列 (angle - angle_zp)，单位 度
        time_stamps: 相对时间序列，单位 秒
        overshoot_threshold: 允许的最大超调（度），超过则额外惩罚
    
    返回:
        score: 越小越好
    """
    if len(angle_error) < 10:
        return 1e6  # 数据太少，视为失败

    dt = np.mean(np.diff(time_stamps)) if len(time_stamps) > 1 else 0.02
    overshoot = np.max(np.abs(angle_error))
    # 最后 3 秒作为稳态
    steady = np.mean(np.abs(angle_error[-int(3.0/dt):])) if len(angle_error) > int(3.0/dt) else overshoot

    # ITAE
    itae = np.sum(time_stamps * np.abs(angle_error) * dt)

    # 惩罚项
    penalty = 0
    if overshoot > overshoot_threshold:
        penalty += (overshoot - overshoot_threshold) * 10.0
    penalty += steady * 5.0

    return itae + penalty