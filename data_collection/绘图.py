#!/usr/bin/env python3
"""
分析已有 CSV 数据：绘制美观的时间响应曲线，计算性能指标。
自动处理中文字体，避免乱码。
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib
import sys
import os

# ==================== 中文字体设置 ====================
# 尝试多个常见的 Linux/macOS/Windows 中文字体，选第一个可用的
for font_name in ['WenQuanYi Micro Hei', 'SimHei', 'Microsoft YaHei', 'Noto Sans CJK SC', 'PingFang SC', 'AR PL UMing CN']:
    try:
        matplotlib.font_manager.findfont(font_name, fallback_to_default=False)
        plt.rcParams['font.sans-serif'] = [font_name]
        print(f"使用字体：{font_name}")
        break
    except:
        continue
else:
    print("警告：未找到中文字体，将使用英文显示，或可能出现方块。")

plt.rcParams['axes.unicode_minus'] = False  # 解决负号显示问题

# 自定义美观风格
plt.rcParams.update({
    'figure.dpi': 120,
    'savefig.dpi': 150,
    'font.size': 10,
    'axes.titlesize': 13,
    'axes.labelsize': 11,
    'legend.fontsize': 9,
    'xtick.labelsize': 9,
    'ytick.labelsize': 9,
    'grid.alpha': 0.3,
    'lines.linewidth': 1.5,
})

# ==================== 配置 ====================
CSV_FILE = "robot_data_20260512_000526.csv"   # 改成你的实际文件名
OUT_DIR = "plots"                              # 图表保存目录

# 创建保存目录
os.makedirs(OUT_DIR, exist_ok=True)

# ==================== 读取数据 ====================
try:
    df = pd.read_csv(CSV_FILE)
except FileNotFoundError:
    print(f"错误：找不到文件 {CSV_FILE}")
    sys.exit(1)

print(f"成功读取 {len(df)} 行数据。")

# ==================== 预处理 ====================
t0 = df['t'].iloc[0]
df['time_s'] = (df['t'] - t0) / 1000.0          # 相对秒数
df['angle_error'] = df['angle'] - df['angle_zp'] # 角度误差
df['abs_error']   = df['angle_error'].abs()

# ==================== 性能指标计算 ====================
dt = np.mean(np.diff(df['time_s'])) if len(df) > 1 else 0.02
mask_steady = df['time_s'] >= (df['time_s'].max() - 3.0)

# ITAE
itae = np.sum(df['time_s'] * df['abs_error'] * dt)

# 超调量
overshoot = df['abs_error'].max()

# 稳态误差
steady_err = df.loc[mask_steady, 'abs_error'].mean() if mask_steady.sum() > 0 else overshoot

# 调节时间（最后一次超出±2°的时刻）
mask_large = df['abs_error'] > 2.0
if mask_large.any():
    settling_time = df.loc[mask_large, 'time_s'].iloc[-1]
else:
    settling_time = 0.0

# 打印指标
print("\n========== 性能基线（原始参数） ==========")
print(f"ITAE              : {itae:.2f}")
print(f"超调量（最大误差）: {overshoot:.2f}°")
print(f"稳态误差          : {steady_err:.2f}°")
print(f"调节时间（±2°）   : {settling_time:.2f} s")
print(f"数据时长          : {df['time_s'].max():.1f} s")
print("============================================\n")

# ==================== 颜色方案 ====================
C_ANGLE   = '#1f77b4'   # 蓝色
C_ZP      = '#d62728'   # 红色
C_ERROR   = '#9467bd'   # 紫色
C_SPEED   = '#2ca02c'   # 绿色
C_LQR     = '#17becf'   # 青色
C_BAT     = '#ff7f0e'   # 橙色
C_HEIGHT  = '#8c564b'   # 棕色

# ==================== 图1：核心状态（角度 + 控制量） ====================
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 9), sharex=True)

# 子图1：角度与零点
ax1.plot(df['time_s'], df['angle'], color=C_ANGLE, alpha=0.85, label='机身角度')
ax1.plot(df['time_s'], df['angle_zp'], color=C_ZP, linestyle='--', linewidth=2, alpha=0.7, label='角度零点')
ax1.fill_between(df['time_s'], df['angle_zp']-2, df['angle_zp']+2, alpha=0.1, color='gray', label='±2° 死区')
ax1.set_ylabel('角度 (度)')
ax1.legend(loc='upper right', ncol=3)
ax1.grid(True)

# 子图2：角度误差与 LQR 控制量 (双 Y 轴)
color_err = C_ERROR
color_lqr = C_LQR
ax2.plot(df['time_s'], df['angle_error'], color=color_err, alpha=0.9, label='角度误差')
ax2.set_ylabel('角度误差 (度)', color=color_err)
ax2.tick_params(axis='y', labelcolor=color_err)
ax2.grid(True)

ax2b = ax2.twinx()
ax2b.plot(df['time_s'], df['LQR_u'], color=color_lqr, alpha=0.6, linestyle='-', linewidth=1.0, label='LQR_u 控制量')
ax2b.set_ylabel('LQR_u 控制量', color=color_lqr)
ax2b.tick_params(axis='y', labelcolor=color_lqr)

# 合并图例
lines1, labels1 = ax2.get_legend_handles_labels()
lines2, labels2 = ax2b.get_legend_handles_labels()
ax2.legend(lines1 + lines2, labels1 + labels2, loc='upper right')

ax2.set_xlabel('时间 (s)')
fig.suptitle('平衡小车原始参数性能（基线）', fontsize=15, y=0.98)
plt.tight_layout()
plt.savefig(os.path.join(OUT_DIR, 'baseline_angle_and_control.png'), dpi=150, bbox_inches='tight')
plt.show()

# ==================== 图2：辅助数据（速度、电压、高度） ====================
fig, (ax3, ax4, ax5) = plt.subplots(3, 1, figsize=(12, 10), sharex=True)

ax3.plot(df['time_s'], df['speed'], color=C_SPEED, label='轮部平均速度')
ax3.set_ylabel('速度 (rad/s)')
ax3.legend(loc='upper right')
ax3.grid(True)

ax4.plot(df['time_s'], df['bat'], color=C_BAT, linewidth=2, label='电池电压')
ax4.set_ylabel('电池电压 (V)')
ax4.legend(loc='upper right')
ax4.grid(True)

ax5.plot(df['time_s'], df['height'], color=C_HEIGHT, linewidth=2, label='设定高度')
ax5.set_xlabel('时间 (s)')
ax5.set_ylabel('高度 (mm)')
ax5.legend(loc='upper right')
ax5.grid(True)

fig.suptitle('辅助状态数据', fontsize=14, y=0.98)
plt.tight_layout()
plt.savefig(os.path.join(OUT_DIR, 'baseline_auxiliary.png'), dpi=150, bbox_inches='tight')
plt.show()

# ==================== 图3：角度误差特写 + 标注 ====================
plt.figure(figsize=(12, 6))
plt.plot(df['time_s'], df['angle_error'], color=C_ERROR, alpha=0.9, label='角度误差')
plt.axhline(y=0, color='gray', linestyle='--', linewidth=1)

# 标记超调点
idx_max = df['abs_error'].idxmax()
t_max = df['time_s'].iloc[idx_max]
e_max = df['angle_error'].iloc[idx_max]
plt.plot(t_max, e_max, 'o', color='red', markersize=10, label=f'超调 {overshoot:.2f}°')
plt.annotate(f'{overshoot:.2f}°', (t_max, e_max), textcoords="offset points",
             xytext=(10,10), fontsize=10, color='red')

# 标记稳态区域
t_end = df['time_s'].max()
plt.axvspan(t_end - 3.0, t_end, alpha=0.15, color='green', label=f'稳态区 (均值 {steady_err:.2f}°)')

plt.xlabel('时间 (s)')
plt.ylabel('角度误差 (度)')
plt.title(f'角度误差响应曲线 | ITAE={itae:.1f} | 超调={overshoot:.2f}° | 稳态={steady_err:.2f}°')
plt.legend(loc='upper right')
plt.grid(True)
plt.tight_layout()
plt.savefig(os.path.join(OUT_DIR, 'baseline_error_detail.png'), dpi=150, bbox_inches='tight')
plt.show()

print(f"图表已保存至目录：{OUT_DIR}/")