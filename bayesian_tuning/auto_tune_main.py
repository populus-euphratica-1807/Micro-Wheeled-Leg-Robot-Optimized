#!/usr/bin/env python3
"""
平衡机器人 PID 参数贝叶斯优化主程序（扩展 I_angle + 分段评价）
- 采集 8 秒，前 3 秒评估恢复，后 3 秒评估稳态。
- 脉冲扰动由 ESP32 执行。
- 搜索空间包含角度环积分增益 I_angle。
- 评价权重已调整，扩大得分分布。
- 优化：移除虚假平滑、新增best-so-far曲线、失效惩罚分机制
"""

import os
import sys
import time
import csv
import logging
import traceback
from datetime import datetime
import numpy as np
import matplotlib.pyplot as plt

import matplotlib
matplotlib.rcParams['font.sans-serif'] = [
    'SimHei', 'Microsoft YaHei', 'WenQuanYi Micro Hei',
    'Noto Sans CJK SC', 'DejaVu Sans'
]
matplotlib.rcParams['axes.unicode_minus'] = False

from ws_client import RobotWS
from udp_collector import UDPCollector
from evaluator import compute_score, DEFAULT_WEIGHTS
from mybayes_opt import create_optimizer, ExperimentAbortError

# ==================== 配置常量 ====================
ESP_IP = "192.168.1.11"
UDP_PORT = 12345
TEST_DURATION = 8.0          # 总采集时长（秒）
SETTLE_TIME = 5.0            # 切参后稳定时间
RECOVERY_DURATION = 3.0      # 恢复段（从 0 秒开始）
STEADY_DURATION = 3.0        # 稳态段（末尾 3 秒）
N_INIT_POINTS = 5
N_ITER = 25
CONSECUTIVE_FAIL_LIMIT = 5
FAIL_PENALTY_SCORE = 20.0    # 失效/摔倒惩罚分，替代极端0分

# 安全参数（含 I_angle = 0 以保证启动安全）
SAFE_PARAMS = {
    'P_angle': 1.0,
    'I_angle': 0.0,
    'P_gyro': 0.06,
    'P_lqr_u': 1.0,
    'I_lqr_u': 15.0
}

# 搜索空间（根据实验结果收紧边界，规避易失稳区域）
SEARCH_SPACE = {
    "P_angle": (0.6, 1.2),
    "I_angle": (0.0, 0.3),
    "P_gyro": (0.06, 0.12),
    "P_lqr_u": (0.8, 1.3),
    "I_lqr_u": (8.0, 18.0)
}

SCORE_WEIGHTS = DEFAULT_WEIGHTS.copy()

# 扰动参数
PERTURB_OFFSET_DEG = 2.5
PERTURB_DURATION_MS = 80


class OptimizationExperiment:
    def __init__(self):
        self.ws = None
        self.udp = None
        self.optimizer = None
        self.results = []
        self.best_safe_params = SAFE_PARAMS.copy()
        self.best_safe_score = 0.0
        self.baseline_wave = None
        self.best_wave = None
        self.best_params_result = None
        self.experiment_counter = 0
        self.consecutive_fails = 0

        self.output_root = "optimization_output_" + datetime.now().strftime("%Y%m%d_%H%M%S")
        self._create_dirs()
        self._setup_logging()
        logging.info(f"输出目录: {self.output_root}")

    def _setup_logging(self):
        log_format = '[%(name)s] %(levelname)s: %(message)s'
        logging.basicConfig(level=logging.INFO, format=log_format,
                            handlers=[
                                logging.StreamHandler(sys.stdout),
                                logging.FileHandler(os.path.join(self.output_root, "run.log"), encoding='utf-8')
                            ])

    def _create_dirs(self):
        self.step_dir = os.path.join(self.output_root, "step_responses")
        self.comp_dir = os.path.join(self.output_root, "comparison")
        self.trend_dir = os.path.join(self.output_root, "optimization_trend")
        for d in [self.step_dir, self.comp_dir, self.trend_dir]:
            os.makedirs(d, exist_ok=True)
        print(f"输出目录: {self.output_root}")

    def _print_param_table(self, P_angle, I_angle, P_gyro, P_lqr_u, I_lqr_u):
        line = (
            f"| P_angle  | {P_angle:>8.4f} "
            f"| I_angle  | {I_angle:>8.4f} "
            f"| P_gyro   | {P_gyro:>8.4f} "
            f"| P_lqr_u  | {P_lqr_u:>8.4f} "
            f"| I_lqr_u  | {I_lqr_u:>8.4f} |"
        )
        logging.info(line)

    def _print_score_table(self, score, info):
        line = (
            f"| 得分   | {score:>6.1f}/100 "
            f"| ITAE   | {info['itae']:>8.2f} "
            f"| 超调(°) | {info['overshoot_deg']:>8.2f} "
            f"| 振荡   | {info['oscillations']:>5d} "
            f"| 稳态σ  | {info['steady_error_std']:>8.3f} "
            f"| 抖动   | {info['torque_jerk']:>8.4f} |"
        )
        logging.info(line)

    def _prepare_robot(self):
        logging.info("检查机器人状态...")
        while True:
            r = self.udp.latest_record
            if r is None:
                time.sleep(0.5)
                continue
            angle = r.get('angle', 0)
            angle_zp = r.get('angle_zp', 0)
            if abs(angle - angle_zp) > 30.0 or abs(angle) > 15.0:
                logging.warning("机器人不稳定，下发最佳安全参数")
                try:
                    self.ws.set_pid('A', P=self.best_safe_params['P_angle'],
                                    I=self.best_safe_params.get('I_angle', 0))
                    self.ws.set_pid('B', P=self.best_safe_params['P_gyro'])
                    self.ws.set_pid('H', P=self.best_safe_params['P_lqr_u'],
                                    I=self.best_safe_params['I_lqr_u'])
                except ConnectionError:
                    logging.error("WebSocket 通信失败，5秒后重试")
                    time.sleep(5)
                    continue
                logging.info("请扶正机器人，等待倾角<5°且绝对角度<15°持续3秒...")
                stable_start = None
                while True:
                    time.sleep(0.2)
                    r2 = self.udp.latest_record
                    if r2 is None:
                        continue
                    a2 = r2.get('angle', 0)
                    a_zp2 = r2.get('angle_zp', 0)
                    if abs(a2 - a_zp2) < 5.0 and abs(a2) < 15.0:
                        if stable_start is None:
                            stable_start = time.time()
                        elif time.time() - stable_start >= 3.0:
                            logging.info("机器人已稳定")
                            break
                    else:
                        stable_start = None
                continue
            break

    def evaluate_params(self, P_angle, I_angle, P_gyro, P_lqr_u, I_lqr_u):
        self._prepare_robot()

        # 渐变切换 PID
        try:
            mid_A = (self.best_safe_params['P_angle'] + P_angle) / 2
            mid_A_I = (self.best_safe_params.get('I_angle', 0) + I_angle) / 2
            mid_B = (self.best_safe_params['P_gyro'] + P_gyro) / 2
            mid_H = (self.best_safe_params['P_lqr_u'] + P_lqr_u) / 2
            mid_I = (self.best_safe_params['I_lqr_u'] + I_lqr_u) / 2
            self.ws.set_pid('A', P=mid_A, I=mid_A_I)
            self.ws.set_pid('B', P=mid_B)
            self.ws.set_pid('H', P=mid_H, I=mid_I)
            time.sleep(1.5)

            self.ws.set_pid('A', P=float(P_angle), I=float(I_angle))
            self.ws.set_pid('B', P=float(P_gyro))
            self.ws.set_pid('H', P=float(P_lqr_u), I=float(I_lqr_u))
        except ConnectionError:
            logging.error("PID 下发失败，放弃本次实验")
            return FAIL_PENALTY_SCORE

        logging.info("机器人已更新到测试参数")
        logging.info(f"等待 {SETTLE_TIME:.0f} 秒稳定...")
        time.sleep(SETTLE_TIME)

        # 施加脉冲扰动
        try:
            logging.info(f"施加脉冲扰动：偏移 {PERTURB_OFFSET_DEG}°，持续 {PERTURB_DURATION_MS} ms")
            self.ws.send_perturb(PERTURB_OFFSET_DEG, PERTURB_DURATION_MS)
            time.sleep(PERTURB_DURATION_MS / 1000.0 + 0.05)
        except ConnectionError:
            logging.error("施加扰动失败")
            return FAIL_PENALTY_SCORE

        logging.info("扰动结束，开始采集恢复数据...")
        angle_err, t_stamps, failed = self.udp.collect(
            TEST_DURATION, angle_threshold=45.0, gyro_threshold=100.0, consecutive_checks=10
        )
        if failed or angle_err is None:
            logging.warning("摔倒/失败，返回惩罚分")
            return FAIL_PENALTY_SCORE

        gyro_vals = [r.get('gyro', 0) for r in self.udp.last_records]
        if np.std(angle_err) < 0.02 and np.std(gyro_vals) < 0.5:
            logging.warning("机器人无有效动作，返回惩罚分")
            return FAIL_PENALTY_SCORE

        lqr_u = np.array([r.get('LQR_u', 0) for r in self.udp.last_records])
        score, info = compute_score(angle_err, t_stamps, lqr_u,
                                    weights=SCORE_WEIGHTS,
                                    recovery_dur=RECOVERY_DURATION,
                                    steady_dur=STEADY_DURATION)

        self._print_score_table(score, info)

        params = {'P_angle': P_angle, 'I_angle': I_angle,
                  'P_gyro': P_gyro, 'P_lqr_u': P_lqr_u, 'I_lqr_u': I_lqr_u}
        self.results.append((params, score, info, angle_err, t_stamps))
        self._save_single_experiment(params, score, info, angle_err, t_stamps)

        if score > self.best_safe_score:
            self.best_safe_score = score
            self.best_safe_params = params.copy()
            self.best_wave = (angle_err.copy(), t_stamps.copy())
            self.best_params_result = params.copy()
            logging.info("已更新最优安全参数")

        return score

    def _target_function(self, P_angle, I_angle, P_gyro, P_lqr_u, I_lqr_u):
        self.experiment_counter += 1
        exp_num = self.experiment_counter
        logging.info("\n" + "="*60)
        logging.info(f"第 {exp_num} 次实验")
        logging.info("="*60)
        self._print_param_table(P_angle, I_angle, P_gyro, P_lqr_u, I_lqr_u)

        try:
            score = self.evaluate_params(P_angle, I_angle, P_gyro, P_lqr_u, I_lqr_u)
        except Exception as e:
            logging.exception(f"实验 {exp_num} 异常")
            score = FAIL_PENALTY_SCORE

        # 连续失败判断：惩罚分视为失效实验
        if score <= FAIL_PENALTY_SCORE:
            self.consecutive_fails += 1
            if self.consecutive_fails >= CONSECUTIVE_FAIL_LIMIT:
                logging.critical("连续失败次数达到上限，终止优化")
                raise ExperimentAbortError("连续失败自动终止")
        else:
            self.consecutive_fails = 0

        logging.info(f">>> 第 {exp_num} 次实验结束 <<<\n")
        return score

    def _save_single_experiment(self, params, score, info, angle_err, t_stamps):
        idx = self.experiment_counter
        try:
            csv_path = os.path.join(self.step_dir, f"test_{idx:02d}.csv")
            np.savetxt(csv_path, np.column_stack((t_stamps, angle_err)),
                       delimiter=",", header="time_s,angle_error_deg", comments="")

            txt_path = os.path.join(self.step_dir, f"test_{idx:02d}_params.txt")
            with open(txt_path, "w", encoding="utf-8") as f:
                f.write(f"P_angle={params['P_angle']:.4f}\n")
                f.write(f"I_angle={params['I_angle']:.4f}\n")
                f.write(f"P_gyro={params['P_gyro']:.4f}\n")
                f.write(f"P_lqr_u={params['P_lqr_u']:.4f}\n")
                f.write(f"I_lqr_u={params['I_lqr_u']:.4f}\n")
                f.write(f"score={score:.2f}\n")
                f.write(f"itae={info['itae']:.2f}\n")
                f.write(f"overshoot_deg={info['overshoot_deg']:.2f}\n")
                f.write(f"oscillations={info['oscillations']}\n")
                f.write(f"steady_error_std={info['steady_error_std']:.4f}\n")
                f.write(f"torque_jerk={info['torque_jerk']:.4f}\n")
        except Exception as e:
            logging.error(f"保存单次数据失败: {e}")

        try:
            plt.figure(figsize=(10, 5))
            plt.plot(t_stamps, angle_err, linewidth=1.0, color='blue')
            plt.axvline(RECOVERY_DURATION, color='orange', linestyle='--', label='恢复/稳态分界')
            plt.axhline(0, color='gray', linestyle='--', alpha=0.6)
            plt.xlabel("时间 (秒)")
            plt.ylabel("角度误差 (度)")
            plt.title(f"实验 {idx:02d} 阶跃响应 (得分={score:.1f})")
            plt.legend()
            plt.grid(True, alpha=0.3)
            plt.tight_layout()
            plt.savefig(os.path.join(self.step_dir, f"test_{idx:02d}.png"), dpi=150)
            plt.close()
        except Exception as e:
            logging.error(f"绘图失败: {e}")

    def _save_final_report(self):
        logging.info("生成最终报告...")
        if self.baseline_wave and self.best_wave:
            try:
                base_err, base_t = self.baseline_wave
                best_err, best_t = self.best_wave
                np.savetxt(os.path.join(self.comp_dir, "baseline_data.csv"),
                           np.column_stack((base_t, base_err)), delimiter=",",
                           header="time_s,angle_error_deg", comments="")
                np.savetxt(os.path.join(self.comp_dir, "best_data.csv"),
                           np.column_stack((best_t, best_err)), delimiter=",",
                           header="time_s,angle_error_deg", comments="")
                with open(os.path.join(self.comp_dir, "comparison_info.txt"), "w", encoding="utf-8") as f:
                    f.write("=== 基线参数 ===\n")
                    for k, v in SAFE_PARAMS.items():
                        f.write(f"{k}={v}\n")
                    f.write("\n=== 最优参数 ===\n")
                    for k, v in self.best_params_result.items():
                        f.write(f"{k}={v}\n")

                plt.figure(figsize=(10, 6))
                plt.plot(base_t, base_err, 'b-', linewidth=1.0, label='基线 (安全参数)')
                plt.plot(best_t, best_err, 'r-', linewidth=1.0, label='最优参数')
                plt.axvline(RECOVERY_DURATION, color='orange', linestyle='--')
                plt.axhline(0, color='gray', linestyle='--')
                plt.xlabel("时间 (秒)")
                plt.ylabel("角度误差 (度)")
                plt.title("对比：基线参数 vs 最优参数")
                plt.legend()
                plt.grid(True, alpha=0.3)
                plt.tight_layout()
                plt.savefig(os.path.join(self.comp_dir, "comparison.png"), dpi=150)
                plt.close()
            except Exception as e:
                logging.error(f"生成对比图失败: {e}")

        if self.results:
            try:
                trend_csv = os.path.join(self.trend_dir, "trend_data.csv")
                with open(trend_csv, "w", newline="", encoding="utf-8") as f:
                    writer = csv.writer(f)
                    writer.writerow(["test_index", "P_angle", "I_angle", "P_gyro", "P_lqr_u", "I_lqr_u",
                                     "score", "itae", "overshoot_deg", "oscillations",
                                     "steady_error_std", "torque_jerk", "recovery_penalty", "steady_penalty"])
                    for i, (params, score, info, _, _) in enumerate(self.results):
                        writer.writerow([i+1,
                                         params['P_angle'], params['I_angle'],
                                         params['P_gyro'], params['P_lqr_u'], params['I_lqr_u'],
                                         score, info['itae'], info['overshoot_deg'],
                                         info['oscillations'], info['steady_error_std'],
                                         info['torque_jerk'],
                                         info.get('recovery_penalty', 0),
                                         info.get('steady_penalty', 0)])

                scores = [r[1] for r in self.results]
                itaes = [r[2]['itae'] for r in self.results]
                overshoots = [r[2]['overshoot_deg'] for r in self.results]
                oscs = [r[2]['oscillations'] for r in self.results]
                stds = [r[2]['steady_error_std'] for r in self.results]
                jerks = [r[2]['torque_jerk'] for r in self.results]

                x_orig = np.arange(len(scores))
                best_so_far = np.maximum.accumulate(scores)  # 历史最优得分

                fig, axes = plt.subplots(2, 3, figsize=(15, 8))
                fig.suptitle("贝叶斯优化过程", fontsize=16, fontweight='bold')

                # 1. 得分趋势：单次得分 + 历史最优
                ax = axes[0, 0]
                ax.plot(x_orig, scores, 'b-o', markersize=3, linewidth=1, label='单次得分', alpha=0.7)
                ax.plot(x_orig, best_so_far, 'r-', linewidth=2, label='历史最优')
                ax.set_title("得分 (0-100)")
                ax.legend()
                ax.grid(True, alpha=0.3)

                # 2. ITAE
                ax = axes[0, 1]
                ax.plot(x_orig, itaes, 'r-o', markersize=3, linewidth=1, label='ITAE', alpha=0.7)
                ax.set_title("ITAE")
                ax.legend()
                ax.grid(True, alpha=0.3)

                # 3. 最大超调
                ax = axes[0, 2]
                ax.plot(x_orig, overshoots, color='orange', marker='o', markersize=3,
                        linewidth=1, label='超调 (°)', alpha=0.7)
                ax.set_title("最大超调")
                ax.legend()
                ax.grid(True, alpha=0.3)

                # 4. 振荡次数
                ax = axes[1, 0]
                ax.plot(x_orig, oscs, 'g-o', markersize=3, linewidth=1, label='振荡次数', alpha=0.7)
                ax.set_title("振荡次数")
                ax.legend()
                ax.grid(True, alpha=0.3)

                # 5. 稳态误差标准差
                ax = axes[1, 1]
                ax.plot(x_orig, stds, color='brown', marker='o', markersize=3,
                        linewidth=1, label='稳态误差 std', alpha=0.7)
                ax.set_title("稳态误差标准差")
                ax.legend()
                ax.grid(True, alpha=0.3)

                # 6. 力矩抖动
                ax = axes[1, 2]
                ax.plot(x_orig, jerks, color='purple', marker='o', markersize=3,
                        linewidth=1, label='力矩抖动', alpha=0.7)
                ax.set_title("力矩抖动")
                ax.legend()
                ax.grid(True, alpha=0.3)

                plt.tight_layout()
                plt.savefig(os.path.join(self.trend_dir, "optimization_trend.png"), dpi=150)
                plt.close()
            except Exception as e:
                logging.error(f"趋势图生成失败: {e}")

        log_csv = os.path.join(self.output_root, "optimization_log.csv")
        try:
            with open(log_csv, "w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow(["test_index", "P_angle", "I_angle", "P_gyro", "P_lqr_u", "I_lqr_u", "score",
                                 "itae", "overshoot", "oscillations", "steady_error_std", "torque_jerk"])
                for i, (params, score, info, _, _) in enumerate(self.results):
                    writer.writerow([i+1,
                                     params['P_angle'], params['I_angle'],
                                     params['P_gyro'], params['P_lqr_u'], params['I_lqr_u'],
                                     score, info['itae'], info['overshoot_deg'],
                                     info['oscillations'], info['steady_error_std'],
                                     info['torque_jerk']])
        except Exception as e:
            logging.error(f"优化日志保存失败: {e}")

    def run(self):
        try:
            self.udp = UDPCollector(UDP_PORT)
            self.udp.start()
            self.ws = RobotWS(ESP_IP)
            self.ws.connect()
            time.sleep(1)

            logging.info("=== 基线测试（安全参数） ===")
            self._print_param_table(**SAFE_PARAMS)
            base_score = self.evaluate_params(**SAFE_PARAMS)
            if base_score > FAIL_PENALTY_SCORE and self.results:
                self.optimizer = create_optimizer(self._target_function, SEARCH_SPACE,
                                                  random_state=42, verbose=0)
                self.optimizer.register(params=SAFE_PARAMS, target=base_score)
                self.baseline_wave = (self.results[-1][3].copy(), self.results[-1][4].copy())
            else:
                logging.warning("基线测试失败，继续优化")
                self.optimizer = create_optimizer(self._target_function, SEARCH_SPACE,
                                                  random_state=42, verbose=0)

            logging.info("开始贝叶斯优化...")
            self.optimizer.maximize(init_points=N_INIT_POINTS, n_iter=N_ITER)

        except ExperimentAbortError:
            logging.warning("优化过程因连续失败自动终止")
        except KeyboardInterrupt:
            logging.warning("用户中断 (Ctrl+C)")
        except Exception as e:
            logging.exception("运行异常")
        finally:
            self._save_final_report()
            if self.optimizer and self.optimizer.max:
                best = self.optimizer.max
                logging.info("=" * 60)
                logging.info(f"最优参数: A.P={best['params']['P_angle']:.4f}, "
                             f"A.I={best['params']['I_angle']:.4f}, "
                             f"B.P={best['params']['P_gyro']:.4f}, "
                             f"H.P={best['params']['P_lqr_u']:.4f}, "
                             f"H.I={best['params']['I_lqr_u']:.2f}")
                logging.info(f"最优得分: {best['target']:.1f}/100")
            if self.udp:
                self.udp.stop()
            if self.ws:
                self.ws.close()
            logging.info(f"所有结果已保存至 {self.output_root}")


if __name__ == "__main__":
    try:
        exp = OptimizationExperiment()
        exp.run()
    except Exception:
        traceback.print_exc()