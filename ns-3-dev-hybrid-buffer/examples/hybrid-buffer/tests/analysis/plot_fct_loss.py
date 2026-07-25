#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np

# =========================
# 1) 数据
# =========================
schemes = [
    'DeepHir-0.2M',
    'DeepHir-0.5M',
    'DeepHir-1.0M',
    'DeepHir-2.0M',
    'DeepHir-4.0M',
    'PBS'
]

x = np.arange(len(schemes))

# Packet loss -> /1e3
packet_loss = np.array([66016.0, 31278.0, 20834.0, 20909.0, 20909.0, 20993.0]) / 1e3
avg_fct     = np.array([5.566, 3.973, 2.669, 2.619, 2.619, 1.645])
p95_fct     = np.array([15.095, 10.244, 5.250, 5.252, 5.252, 5.270])

# =========================
# 2) 为避免三条线重叠，x 轻微错开
# =========================
offset = 0.06
x_loss = x - offset
x_avg  = x
x_p95  = x + offset

# =========================
# 3) 全局风格
# =========================
plt.rcParams['font.family'] = 'serif'
plt.rcParams['font.size'] = 16
plt.rcParams['axes.linewidth'] = 1.6
plt.rcParams['xtick.major.width'] = 1.2
plt.rcParams['ytick.major.width'] = 1.2
plt.rcParams['xtick.direction'] = 'in'
plt.rcParams['ytick.direction'] = 'in'

# =========================
# 4) 创建图像
# =========================
fig, ax1 = plt.subplots(figsize=(10.5, 6.5), dpi=300)
fig.patch.set_facecolor('white')
ax1.set_facecolor('white')
ax1.grid(False)

# =========================
# 5) 左轴：Packet Loss
# =========================
line1, = ax1.plot(
    x_loss, packet_loss,
    color='red',
    marker='o',
    markersize=10,              # 点更大
    markerfacecolor='red',
    markeredgecolor='red',
    markeredgewidth=1.6,
    linewidth=2.8,              # 线更粗
    label='Packet Loss',
    zorder=3
)

ax1.set_xlabel('')
ax1.set_ylabel(r'Packet Loss ($\times 10^3$)', fontsize=20, color='black')

ax1.set_xticks(x)
ax1.set_xticklabels(schemes, fontsize=15, color='black', rotation=12, ha='right')
ax1.tick_params(axis='x', labelsize=15, colors='black')
ax1.tick_params(axis='y', labelsize=16, colors='black')

# 给左轴留出顶部空间，防止标注超出
ax1.set_ylim(20, 70)

# =========================
# 6) 右轴：FCT
# =========================
ax2 = ax1.twinx()

line2, = ax2.plot(
    x_avg, avg_fct,
    color='blue',
    marker='^',
    markersize=10,
    markerfacecolor='blue',
    markeredgecolor='blue',
    markeredgewidth=1.6,
    linewidth=2.8,
    label='Avg FCT',
    zorder=4
)

line3, = ax2.plot(
    x_p95, p95_fct,
    color='green',
    marker='s',
    markersize=10,
    markerfacecolor='green',
    markeredgecolor='green',
    markeredgewidth=1.6,
    linewidth=2.8,
    label='P95 FCT',
    zorder=4
)

ax2.set_ylabel('FCT (ms)', fontsize=20, color='black')
ax2.tick_params(axis='y', labelsize=16, colors='black')

# 给右轴留出顶部空间，防止绿色第一个点文字跑出去
ax2.set_ylim(0.8, 15.7)

# =========================
# 7) 数据标注：分别设置偏移，避免叠在一起
# =========================
# 红色 Packet Loss 标注偏移
loss_dx = np.array([-0.02, -0.04,  0.00, -0.02, -0.02,  0.00])
loss_dy = np.array([ 0.70,  0.60,  0.55,  0.45,  0.45,  0.45])

# 蓝色 Avg FCT 标注偏移
avg_dx = np.array([-0.06, -0.02,  0.03,  0.04,  0.04,  0.03])
avg_dy = np.array([ 0.18,  0.14,  0.12,  0.12,  0.12,  0.12])

# 绿色 P95 FCT 标注偏移
p95_dx = np.array([-0.10,  0.06,  0.06,  0.06,  0.06,  0.00])
p95_dy = np.array([ 0.45,  0.35,  0.35,  0.35,  0.35,  0.35])

for i, (xi, yi) in enumerate(zip(x_loss, packet_loss)):
    ax1.text(
        xi + loss_dx[i],
        yi + loss_dy[i],
        f'{yi:.1f}',
        ha='center',
        va='bottom',
        fontsize=11,
        color='red',
        clip_on=True
    )

for i, (xi, yi) in enumerate(zip(x_avg, avg_fct)):
    ax2.text(
        xi + avg_dx[i],
        yi + avg_dy[i],
        f'{yi:.3f}',
        ha='center',
        va='bottom',
        fontsize=11,
        color='blue',
        clip_on=True
    )

for i, (xi, yi) in enumerate(zip(x_p95, p95_fct)):
    ax2.text(
        xi + p95_dx[i],
        yi + p95_dy[i],
        f'{yi:.3f}',
        ha='center',
        va='bottom',
        fontsize=11,
        color='green',
        clip_on=True
    )

# =========================
# 8) 图例：框内右上角
# =========================
lines = [line1, line2, line3]
labels = [l.get_label() for l in lines]

legend = ax1.legend(
    lines, labels,
    loc='upper right',
    frameon=True,
    fontsize=15,
    edgecolor='black',
    fancybox=False
)
legend.get_frame().set_alpha(1.0)

# =========================
# 9) 边框
# =========================
for spine in ax1.spines.values():
    spine.set_linewidth(1.4)
for spine in ax2.spines.values():
    spine.set_linewidth(1.4)

plt.tight_layout()

# =========================
# 10) 保存
# =========================
plt.savefig('combined_metrics_final_v2.png', dpi=300, bbox_inches='tight')
plt.savefig('combined_metrics_final_v2.pdf', bbox_inches='tight')
plt.show()