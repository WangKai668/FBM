#!/usr/bin/env python3
# coding: utf-8

import matplotlib.pyplot as plt
import numpy as np
import matplotlib
matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42
# =========================
# 1. 数据
# =========================
e_labels = ['0.001','0.01',  '0.1',  '1','10'] # '0.05','0.5',

# 使用分类位置，保证横坐标间距完全相同
x = np.arange(len(e_labels))

avg_fct_ms = np.array([
    1.293,
    1.421,
    1.145,
    1.376,
    1.349,
    # 1.292437,
    # 1.279350
])

packet_loss = np.array([
    10851 ,
    11545,
    8926,
    11180,
    10732,
    # 10586,
    # 10293
])/ 1e3

# rand(1)
# avg_fct_ms = np.array([
#     1.292,
#     1.421,
#     1.145,
#     1.376,
#     1.349,
#     # 1.292437,
#     # 1.279350
# ])

# packet_loss = np.array([
#     10851 ,
#     11545,
#     8926,
#     11180,
#     10732,
#     # 10586,
#     # 10293
# ])/ 1e3
# rand(2)
# avg_fct_ms = np.array([
#     1.081,
#     1.144,
#     1.112,
#     1.300,
#     1.239,
#     # 1.292437,
#     # 1.279350
# ])

# packet_loss = np.array([
#     8111,
#     8587,
#     8760,
#     10557,
#     9731,
#     # 10586,
#     # 10293
# ])/ 1e3


# rand(3)
# avg_fct_ms = np.array([
#     1.551,
#     1.348,
#     1.633,
#     1.488,
#     1.436,
#     # 1.292437,
#     # 1.279350
# ])

# packet_loss = np.array([
#     12805,
#     10759,
#     12511,
#     12179,
#     11035,
#     # 10586,
#     # 10293
# ])/ 1e3


# # rand(4)
# avg_fct_ms = np.array([
#     1.321,
#     1.497,
#     1.306,
#     1.446,
#     1.368,
#     # 1.292437,
#     # 1.279350
# ])

# packet_loss = np.array([
#     10670,
#     11902,
#     10289,
#     12058,
#     10768,
#     # 10586,
#     # 10293
# ])/ 1e3


# rand(5)
# avg_fct_ms = np.array([
#     1.242,
#     1.085,
#     1.232,
#     1.279,
#     1.957,
#     # 1.292437,
#     # 1.279350
# ])

# packet_loss = np.array([
#     9641,
#     8703,
#     10078,
#     9323,
#     7586,
#     # 10586,
#     # 10293
# ])/ 1e3

# # rand(5)
# avg_fct_ms = np.array([1.298156, 1.299384, 1.286140, 1.358470, 1.270231])

# packet_loss = np.array([10415.60, 10299.20, 10092.80, 10859.40, 9970.40])/ 1e3
# =========================
# 2. 全局样式
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
# 3. 创建图像
# =========================
fig, ax1 = plt.subplots(
    figsize=(8, 5),
    dpi=300
)

fig.patch.set_facecolor('white')
ax1.set_facecolor('white')
ax1.grid(False)

# 右侧纵坐标
ax2 = ax1.twinx()

# =========================
# 4. Avg FCT：黑色
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
    r'$\eta$',
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

# 根据数据设置合理范围
ax1.set_ylim(1.0, 2.0)

# =========================
# 5. Packet Loss：红色曲线
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

# 纵坐标标题和刻度均为黑色
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

ax2.set_ylim(7, 15)

# =========================
# 6. 等间距横坐标
# =========================
ax1.set_xticks(x)

ax1.set_xticklabels(
    e_labels,
    fontsize=FINAL_FONT_SIZE,
    fontname='Times New Roman',
    color='black'
)

# 左右两端留白
ax1.set_xlim(
    x[0] - 0.4,
    x[-1] + 0.4
)

# =========================
# 7. 图层顺序
# =========================
ax1.set_zorder(2)
ax2.set_zorder(3)

# 右轴背景透明，避免遮挡黑色FCT曲线
ax2.patch.set_visible(False)

# =========================
# 8. 图例
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
# 9. 坐标轴边框全部使用黑色
# =========================
for spine in ax1.spines.values():
    spine.set_color('black')
    spine.set_linewidth(1.8)

for spine in ax2.spines.values():
    spine.set_color('black')
    spine.set_linewidth(1.8)

# =========================
# 10. 页面布局
# =========================
plt.subplots_adjust(
    left=0.15,
    right=0.85,
    top=0.86,
    bottom=0.20
)

# =========================
# 11. 保存
# =========================
plt.savefig(
    'tc2-03-e-fct-loss.png',
    dpi=300,
    bbox_inches='tight',
    pad_inches=0.05
)

plt.savefig(
    'tc2-03-e-fct-loss.pdf',
    bbox_inches='tight',
    pad_inches=0.05
)
plt.show()