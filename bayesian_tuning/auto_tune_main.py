#!/usr/bin/env python3
import sys, traceback, time, csv
import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import make_interp_spline
from ws_client import RobotWS
from udp_collector import UDPCollector
from evaluator import compute_score
from mybayes_opt import create_optimizer

# ==================== 配置 ====================
ESP_IP = "192.168.1.11"
UDP_PORT = 12345
TEST_DURATION = 15.0
SETTLE_TIME = 5.0
N_INIT_POINTS = 5
N_ITER = 25

SAFE_PARAMS = {
    'P_angle': 1.0,
    'P_gyro':  0.06,
    'P_lqr_u': 1.0,
    'I_lqr_u': 15.0
}

SEARCH_SPACE = {
    "P_angle": (0.6, 1.5),
    "P_gyro":  (0.04, 0.12),
    "P_lqr_u": (0.8, 1.5),
    "I_lqr_u": (8.0, 22.0)
}

# 全局对象
ws = None
udp = None
optimizer = None
results = []
best_safe_params = SAFE_PARAMS.copy()
best_safe_score = 0.0

def prepare_robot():
    print("  >> prepare_robot: 检查状态...", flush=True)
    while True:
        r = udp.latest_record
        if r is None:
            time.sleep(0.5)
            continue
        angle = r.get('angle', 0)
        angle_zp = r.get('angle_zp', 0)
        # 新增：绝对倾角也必须小于 15°，防止倒地后零偏修正欺骗检测
        if abs(angle - angle_zp) > 30.0 or abs(angle) > 15.0:
            print("  >> 机器人不稳定，下发最佳安全参数...", flush=True)
            ws.set_pid('A', P=best_safe_params['P_angle'])
            ws.set_pid('B', P=best_safe_params['P_gyro'])
            ws.set_pid('H', P=best_safe_params['P_lqr_u'], I=best_safe_params['I_lqr_u'])
            print("  >> 请扶正机器人，等待倾角<5°且绝对角度<15°持续3秒...", flush=True)
            stable_start = None
            while True:
                time.sleep(0.2)
                r2 = udp.latest_record
                if r2 is None: continue
                a2 = r2.get('angle', 0)
                a_zp2 = r2.get('angle_zp', 0)
                if abs(a2 - a_zp2) < 5.0 and abs(a2) < 15.0:
                    if stable_start is None:
                        stable_start = time.time()
                    elif time.time() - stable_start >= 3.0:
                        print("  >> 机器人已稳定", flush=True)
                        break
                else:
                    stable_start = None
            continue
        break

def evaluate_params(P_angle, P_gyro, P_lqr_u, I_lqr_u):
    global ws, udp, optimizer, results, best_safe_score, best_safe_params
    prepare_robot()

    # 渐变切换
    mid_A = (best_safe_params['P_angle'] + P_angle) / 2
    mid_B = (best_safe_params['P_gyro'] + P_gyro) / 2
    mid_H = (best_safe_params['P_lqr_u'] + P_lqr_u) / 2
    mid_I = (best_safe_params['I_lqr_u'] + I_lqr_u) / 2
    ws.set_pid('A', P=mid_A)
    ws.set_pid('B', P=mid_B)
    ws.set_pid('H', P=mid_H, I=mid_I)
    time.sleep(1.5)

    ws.set_pid('A', P=float(P_angle))
    ws.set_pid('B', P=float(P_gyro))
    ws.set_pid('H', P=float(P_lqr_u), I=float(I_lqr_u))
    print(f"  >> 等待 {SETTLE_TIME}s 稳定...", flush=True)
    time.sleep(SETTLE_TIME)

    # 使用宽松的摔倒检测
    angle_err, t_stamps, failed = udp.collect(TEST_DURATION, angle_threshold=45.0, gyro_threshold=100.0, consecutive_checks=10)
    if failed or angle_err is None:
        print("  >> 摔倒/失败，得分 = 0", flush=True)
        return 0.0

    # 额外防呆：数据无波动
    if np.std(angle_err) < 0.02 and np.std([r.get('gyro',0) for r in udp.last_records]) < 0.5:
        print("  >> 机器人无有效动作，得分 = 0", flush=True)
        return 0.0

    lqr_u = np.array([r.get('LQR_u', 0) for r in udp.last_records])
    score, info = compute_score(angle_err, t_stamps, lqr_u)
    print(f"  >> 得分 = {score:.1f}/100", flush=True)
    print(f"     ITAE={info['itae']:.2f} 超调={info['overshoot_deg']:.2f}° 振荡={info['oscillations']}次 力矩抖动={info['torque_jerk']:.4f}", flush=True)

    params = {'P_angle': P_angle, 'P_gyro': P_gyro, 'P_lqr_u': P_lqr_u, 'I_lqr_u': I_lqr_u}
    results.append((params, score, info))

    if score > best_safe_score:
        best_safe_score = score
        best_safe_params = params.copy()
        print("  >> 已更新最优安全参数！", flush=True)
    return score

def plot_results(results, safe_params, base_score, base_info):
    if not results:
        print("No data to plot.")
        return

    scores = [r[1] for r in results]
    itaes = [r[2]['itae'] for r in results]
    overshoots = [r[2]['overshoot_deg'] for r in results]
    oscs = [r[2]['oscillations'] for r in results]
    jerks = [r[2]['torque_jerk'] for r in results]

    x_original = np.arange(len(scores))
    x_smooth = np.linspace(x_original.min(), x_original.max(), 300)

    def smooth(y):
        if len(y) < 3:
            return x_original, y
        k = min(3, len(y)-1)
        spl = make_interp_spline(x_original, y, k=k)
        return x_smooth, spl(x_smooth)

    fig, axes = plt.subplots(2, 2, figsize=(12, 8))
    fig.suptitle("Bayesian Optimization Process", fontsize=16, fontweight='bold')

    # Score
    ax = axes[0, 0]
    ax.plot(*smooth(scores), color='blue', label='Score')
    ax.axhline(base_score, color='gray', linestyle='--', label=f'Baseline ({base_score:.1f})')
    ax.set_title("Score (0-100)")
    ax.set_xlabel("Test #")
    ax.legend()

    # ITAE
    ax = axes[0, 1]
    ax.plot(*smooth(itaes), color='red', label='ITAE')
    if base_info:
        ax.axhline(base_info['itae'], color='gray', linestyle='--', label=f'Baseline ({base_info["itae"]:.2f})')
    ax.set_title("ITAE (Lower is Better)")
    ax.legend()

    # Overshoot
    ax = axes[1, 0]
    ax.plot(*smooth(overshoots), color='orange', label='Max Overshoot (°)')
    if base_info:
        ax.axhline(base_info['overshoot_deg'], color='gray', linestyle='--', label=f'Baseline ({base_info["overshoot_deg"]:.2f}°)')
    ax.set_title("Max Overshoot Angle")
    ax.legend()

    # Oscillations & Torque Jerk
    ax1 = axes[1, 1]
    ax1.plot(*smooth(oscs), color='green', label='Oscillations')
    ax1.set_title("Oscillations & Torque Jerk")
    ax1.set_xlabel("Test #")
    ax1.set_ylabel("Oscillations", color='green')
    ax2 = ax1.twinx()
    ax2.plot(*smooth(jerks), color='purple', linestyle='--', label='Torque Jerk')
    ax2.set_ylabel("Torque Jerk", color='purple')
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, loc='best')

    plt.tight_layout()
    plt.savefig("optimization_curve.png", dpi=150)
    print("Curve saved as optimization_curve.png")
    plt.show()

def main():
    global ws, udp, optimizer, best_safe_score, best_safe_params, results

    print("=== 开始主程序 ===", flush=True)
    udp = UDPCollector(UDP_PORT)
    udp.start()
    ws = RobotWS(ESP_IP)
    ws.connect()
    time.sleep(1)

    def target_function(P_angle, P_gyro, P_lqr_u, I_lqr_u):
        return evaluate_params(P_angle, P_gyro, P_lqr_u, I_lqr_u)

    optimizer = create_optimizer(target_function, SEARCH_SPACE, random_state=42)

    # 基线测试
    print("\n=== 测试原始安全参数（基线） ===", flush=True)
    base_score, base_info = 0.0, None
    try:
        prepare_robot()
        ws.set_pid('A', P=SAFE_PARAMS['P_angle'])
        ws.set_pid('B', P=SAFE_PARAMS['P_gyro'])
        ws.set_pid('H', P=SAFE_PARAMS['P_lqr_u'], I=SAFE_PARAMS['I_lqr_u'])
        time.sleep(5.0)
        angle_err, t_stamps, failed = udp.collect(TEST_DURATION, angle_threshold=45.0, gyro_threshold=100.0, consecutive_checks=10)
        if not failed and angle_err is not None:
            lqr_u = np.array([r.get('LQR_u', 0) for r in udp.last_records])
            base_score, base_info = compute_score(angle_err, t_stamps, lqr_u)
            print(f"基线安全参数得分: {base_score:.1f}/100", flush=True)
            optimizer.register(params=SAFE_PARAMS, target=base_score)
        else:
            print("基线测试失败！", flush=True)
    except Exception as e:
        print(f"基线测试异常: {e}", flush=True)

    print("\n开始贝叶斯优化...", flush=True)
    optimizer.maximize(init_points=N_INIT_POINTS, n_iter=N_ITER)

    best = optimizer.max
    best_params = best['params']
    best_target = best['target']

    print("\n" + "=" * 60)
    print("优化完成！")
    print(f"最优参数: A.P={best_params['P_angle']:.4f}, B.P={best_params['P_gyro']:.4f}, "
          f"H.P={best_params['P_lqr_u']:.4f}, H.I={best_params['I_lqr_u']:.2f}")
    print(f"最优得分: {best_target:.1f}/100")

    # 保存CSV
    with open("optimization_log.csv", "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["P_angle", "P_gyro", "P_lqr_u", "I_lqr_u", "score",
                         "itae", "overshoot", "oscillations", "torque_jerk"])
        for params, score, info in results:
            writer.writerow([params['P_angle'], params['P_gyro'], params['P_lqr_u'], params['I_lqr_u'],
                             score, info['itae'], info['overshoot_deg'], info['oscillations'], info['torque_jerk']])

    plot_results(results, SAFE_PARAMS, base_score, base_info)

    udp.stop()
    ws.close()

if __name__ == "__main__":
    try:
        main()
    except SystemExit:
        pass
    except:
        traceback.print_exc()