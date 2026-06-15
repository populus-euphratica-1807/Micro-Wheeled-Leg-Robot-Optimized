#!/usr/bin/env python3
"""
评价函数：百分制，0 = 摔倒，100 = 完美平衡。
返回总分及各项指标字典，便于记录和绘图。
"""

import numpy as np

def compute_score(angle_error, time_stamps, lqr_u=None):
    if len(angle_error) < 10:
        return 0.0, {"itae": 0, "overshoot": 0, "oscillations": 0, "torque_jerk": 0}

    dt = np.mean(np.diff(time_stamps)) if len(time_stamps) > 1 else 0.02

    # ITAE
    itae = 0.3 * np.sum(time_stamps * np.abs(angle_error) * dt)

    # 超调
    overshoot = np.max(np.abs(angle_error))
    overshoot_penalty = max(0, (overshoot - 3.0)) * 2.0

    # 振荡次数
    sign_changes = np.sum(np.diff(np.sign(angle_error)) != 0)
    oscillation_penalty = sign_changes * 0.2

    # 力矩抖动
    torque_penalty = 0.0
    torque_jerk_val = 0.0
    if lqr_u is not None and len(lqr_u) > 1:
        torque_jerk_val = np.sum(np.diff(lqr_u) ** 2) / len(lqr_u)
        torque_penalty = torque_jerk_val * 0.005

    total_penalty = itae + overshoot_penalty + oscillation_penalty + torque_penalty
    score = 100.0 * np.exp(-0.005 * total_penalty)
    score = max(0.0, min(100.0, score))

    # 返回分解指标（未乘系数的原始值更直观）
    info = {
        "itae": itae,
        "overshoot_deg": overshoot,
        "oscillations": int(sign_changes),
        "torque_jerk": torque_jerk_val,
        "overshoot_penalty": overshoot_penalty,
        "oscillation_penalty": oscillation_penalty,
        "torque_penalty": torque_penalty,
        "total_penalty": total_penalty
    }
    return score, info