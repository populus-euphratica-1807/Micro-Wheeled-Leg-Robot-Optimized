#!/usr/bin/env python3
"""
评价函数模块：分段评价恢复段（前 3 秒）和稳态段（最后 3 秒）
权重已调整，扩大得分分布，避免所有参数接近满分。
"""

import numpy as np

DEFAULT_WEIGHTS = {
    # ---------- 恢复段指标 ----------
    "itae_factor": 1.5,
    "overshoot_threshold_deg": 2.0,
    "overshoot_penalty_per_deg": 5.0,
    "oscillation_penalty_per_change": 0.15,
    "recovery_weight": 0.65,

    # ---------- 稳态段指标 ----------
    "steady_error_std_factor": 10.0,
    "steady_torque_jerk_factor": 1.0,
    "steady_weight": 0.35,

    # ---------- 全局映射 ----------
    "score_decay": 0.03,
}


def compute_score(angle_error, time_stamps, lqr_u=None, weights=None,
                  recovery_dur=3.0, steady_dur=3.0):
    """
    参数:
        angle_error : 角度误差数组（度）
        time_stamps : 时间数组（秒，已相对扰动结束时刻）
        lqr_u       : 控制量 LQR_u（可选）
        weights     : 权重字典
        recovery_dur: 恢复段时长（从0开始）
        steady_dur  : 稳态段时长（从末尾向前截取）
    返回:
        score : 0~100 得分
        info  : 详细指标字典
    """
    if weights is None:
        weights = DEFAULT_WEIGHTS

    if len(angle_error) < 10:
        return 0.0, {
            "itae": 0, "overshoot_deg": 0, "oscillations": 0,
            "steady_error_std": 0, "torque_jerk": 0,
            "total_penalty": 0
        }

    # 时间分段
    t_end = time_stamps[-1]
    rec_mask = time_stamps <= recovery_dur
    std_mask = time_stamps >= (t_end - steady_dur)

    angle_rec = angle_error[rec_mask]
    time_rec = time_stamps[rec_mask]
    angle_std = angle_error[std_mask]
    lqr_std = lqr_u[std_mask] if lqr_u is not None else None

    # ========== 恢复段 ==========
    dt = np.mean(np.diff(time_stamps)) if len(time_stamps) > 1 else 0.02
    itae_raw = np.sum(time_rec * np.abs(angle_rec) * dt) if len(angle_rec) > 0 else 0.0
    itae = weights["itae_factor"] * itae_raw

    overshoot = np.max(np.abs(angle_rec)) if len(angle_rec) > 0 else 0.0
    overshoot_penalty = max(0, (overshoot - weights["overshoot_threshold_deg"])) * \
                        weights["overshoot_penalty_per_deg"]

    sign_changes = np.sum(np.diff(np.sign(angle_rec)) != 0) if len(angle_rec) > 2 else 0
    oscillation_penalty = sign_changes * weights["oscillation_penalty_per_change"]

    recovery_penalty = itae + overshoot_penalty + oscillation_penalty

    # ========== 稳态段 ==========
    steady_error_std = np.std(angle_std) if len(angle_std) > 0 else 0.0
    steady_error_penalty = steady_error_std * weights["steady_error_std_factor"]

    torque_jerk_val = 0.0
    torque_jerk_penalty = 0.0
    if lqr_std is not None and len(lqr_std) > 1:
        torque_jerk_val = np.sum(np.diff(lqr_std) ** 2) / len(lqr_std)
        torque_jerk_penalty = torque_jerk_val * weights["steady_torque_jerk_factor"]

    steady_penalty = steady_error_penalty + torque_jerk_penalty

    # ========== 合成总分 ==========
    total_penalty = (weights["recovery_weight"] * recovery_penalty +
                     weights["steady_weight"] * steady_penalty)
    score = 100.0 * np.exp(-weights["score_decay"] * total_penalty)
    score = max(0.0, min(100.0, score))

    info = {
        "itae": itae,
        "overshoot_deg": overshoot,
        "oscillations": int(sign_changes),
        "recovery_penalty": recovery_penalty,
        "steady_error_std": steady_error_std,
        "torque_jerk": torque_jerk_val,
        "steady_penalty": steady_penalty,
        "total_penalty": total_penalty
    }
    return score, info