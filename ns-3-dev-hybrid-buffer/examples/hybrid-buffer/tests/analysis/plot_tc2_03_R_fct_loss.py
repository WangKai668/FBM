#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import MultipleLocator
import matplotlib
matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

# =========================
# 1. 数据
# =========================
r_labels = ['2', '4', '8', '16', '32']

# 使用分类横坐标，保证各点之间距离相同
x = np.arange(len(r_labels))

avg_fct_ms = np.array([
    1.312607,
    1.297499,
    1.181699,
    1.145082,
    1.309625
])

# 除以1e3，右轴单位为 ×10^3
packet_loss = np.array([
    10693,
    10439,
    9450,
    8926,
    10170
]) / 1e3

# #rand(1)
# avg_fct_ms = np.array([
#     1.312607,
#     1.297499,
#     1.181699,
#     1.145082,
#     1.309625
# ])

# # 除以1e3，右轴单位为 ×10^3
# packet_loss = np.array([
#     10693,
#     10439,
#     9450,
#     8926,
#     10170
# ]) / 1e3

#rand(2)
# avg_fct_ms = np.array([
#     1.219,
#     1.247,
#     1.270,
#     1.112,
#     1.181
# ])

# # 除以1e3，右轴单位为 ×10^3
# packet_loss = np.array([
#     9756,
#     9601,
#     10074,
#     8760,
#     9834
# ]) / 1e3

#rand(3)
# avg_fct_ms = np.array([
#     1.353,
#     1.329,
#     1.349,
#     1.633,
#     1.518
# ])

# # 除以1e3，右轴单位为 ×10^3
# packet_loss = np.array([
#     11486,
#     10583,
#     10825,
#     12511,
#     12267
# ]) / 1e3


#rand(5)
# avg_fct_ms = np.array([
#     1.109,
#     1.094,
#     1.196,
#     1.232,
#     1.061
# ])

# # 除以1e3，右轴单位为 ×10^3
# packet_loss = np.array([
#     9149,
#     8527,
#     9266,
#     10078,
#     8622
# ]) / 1e3

#rand(4)
# avg_fct_ms = np.array([1.226931, 1.285472, 1.304826, 1.286140, 1.286879])

# # 除以1e3，右轴单位为 ×10^3
# packet_loss = np.array([10078.80, 10227.60, 10319.80, 10092.80, 10327.60]) / 1e3



# =========================
# 2. 手动设置坐标轴范围
# =========================

# 左轴：Avg FCT
FCT_Y_MIN = 1.0
FCT_Y_MAX = 1.9
FCT_Y_STEP = 0.3

# 右轴：Packet Loss
LOSS_Y_MIN = 8.0
LOSS_Y_MAX = 12.0
LOSS_Y_STEP = 2.0

# 横坐标左右留白
X_LEFT_MARGIN = 0.4
X_RIGHT_MARGIN = 0.4


# =========================
# 3. 全局绘图风格
# =========================
FINAL_FONT_SIZE = 30

plt.rcParams['font.family'] = 'Times New Roman'
plt.rcParams['mathtext.fontset'] = 'stix'
plt.rcParams['font.size'] = FINAL_FONT_SIZE

plt.rcParams['axes.linewidth'] = 1.8
plt.rcParams['xtick.major.width'] = 1.4
plt.rcParams['ytick.major.width'] = 1.4
plt.rcParams['xtick.direction'] = 'in'
plt.rcParams['ytick.direction'] = 'in'



# =========================
# 4. 创建画布
# =========================
fig, ax1 = plt.subplots(
    figsize=(8, 5),
    dpi=300
)

fig.patch.set_facecolor('white')
ax1.set_facecolor('white')
ax1.grid(False)

# 创建右侧纵坐标
ax2 = ax1.twinx()


# =========================
# 5. 左轴：Avg FCT
# =========================
line_fct, = ax1.plot(
    x,
    avg_fct_ms,
    color='black',
    marker='o',
    markersize=11,
    markerfacecolor='black',
    markeredgecolor='black',
    markeredgewidth=1.2,
    linewidth=3.0,
    label='Avg FCT',
    zorder=5
)

ax1.set_xlabel(
    r'$T_{\mathrm{max}}$ (us)',
    fontsize=40,
    color='black'
)

ax1.set_ylabel(
    'Avg FCT (ms)',
    fontsize=FINAL_FONT_SIZE,
    color='black',
    labelpad=12
)

ax1.tick_params(
    axis='both',
    labelsize=FINAL_FONT_SIZE,
    colors='black',
    width=1.4,
    length=7
)

# 手动设置左侧纵坐标范围
ax1.set_ylim(
    FCT_Y_MIN,
    FCT_Y_MAX
)

# 手动设置左侧纵坐标刻度间隔
ax1.yaxis.set_major_locator(
    MultipleLocator(FCT_Y_STEP)
)


# =========================
# 6. 右轴：Packet Loss
# =========================
line_loss, = ax2.plot(
    x,
    packet_loss,
    color='red',
    marker='s',
    markersize=11,
    markerfacecolor='red',
    markeredgecolor='red',
    markeredgewidth=1.2,
    linewidth=3.0,
    label='Packet Loss',
    zorder=10
)

ax2.set_ylabel(
    r'#of Packet Loss (x1e3)',
    fontsize=28,
    color='black'
)

ax2.tick_params(
    axis='y',
    labelsize=FINAL_FONT_SIZE,
    colors='black',
    width=1.4,
    length=7
)

# 手动设置右侧纵坐标范围
ax2.set_ylim(
    LOSS_Y_MIN,
    LOSS_Y_MAX
)

# 手动设置右侧纵坐标刻度间隔
ax2.yaxis.set_major_locator(
    MultipleLocator(LOSS_Y_STEP)
)


# =========================
# 7. 横坐标
# =========================
ax1.set_xticks(x)

ax1.set_xticklabels(
    r_labels,
    fontsize=FINAL_FONT_SIZE,
    fontname='Times New Roman',
    color='black'
)

# 手动设置横坐标范围
ax1.set_xlim(
    x[0] - X_LEFT_MARGIN,
    x[-1] + X_RIGHT_MARGIN
)


# =========================
# 8. 图层顺序
# =========================
ax1.set_zorder(2)
ax2.set_zorder(3)

# 右轴背景透明，避免遮挡左轴曲线
ax2.patch.set_visible(False)


# =========================
# 9. 图例
# =========================
legend = ax2.legend(
    handles=[line_fct, line_loss],
    labels=['Avg FCT', 'Packet Loss'],
    loc='upper center',
    bbox_to_anchor=(0.5, 1.02),
    ncol=2,
    fontsize=FINAL_FONT_SIZE,
    frameon=False,
    handlelength=1.5,
    handletextpad=0.4,
    columnspacing=1.0
)

legend.set_zorder(20)


# =========================
# 10. 坐标轴边框
# =========================
for spine in ax1.spines.values():
    spine.set_color('black')
    spine.set_linewidth(1.8)

for spine in ax2.spines.values():
    spine.set_color('black')
    spine.set_linewidth(1.8)


# =========================
# 11. 页面布局
# =========================
plt.subplots_adjust(
    left=0.15,
    right=0.85,
    top=0.86,
    bottom=0.20
)


# =========================
# 12. 保存
# =========================
plt.savefig(
    'tc2-03-R-fct-loss.png',
    dpi=300,
    bbox_inches='tight',
    pad_inches=0.05
)

plt.savefig(
    'tc2-03-R-fct-loss.pdf',
    bbox_inches='tight',
    pad_inches=0.05
)

plt.show()