#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import NullLocator

# =========================
# 1) 数据
# =========================
schemes = [
    '1',
    '2',
    '3',
    '4',
    '5'
]

# 增大不同方案之间的横向距离
x_spacing = 1.1
x = np.arange(len(schemes)) * x_spacing

# Packet Loss除以1e3
# w-16
# packet_loss = np.array([48750.0, 23090.0, 11013.0, 11339.0, 11339.0, 8926.0]) / 1e3

# avg_fct = np.array([5.790, 4.187, 2.048, 1.985, 1.985, 1.145])

# p95_fct = np.array([15.058, 5.265, 5.239, 5.240, 5.240, 5.245])

# h-16
packet_loss = np.array([18341, 15159, 20249, 16458, 17796]) / 1e3

avg_fct = np.array([1.587958, 1.256278, 1.601388, 1.454323, 1.497706])

p95_fct = np.array([5.273000, 5.260000, 5.281000, 5.262000, 5.257000])






# 检查数据关系
if np.any(p95_fct < avg_fct):
    raise ValueError('存在95th FCT小于Avg FCT的数据，请检查输入。')
# 95th柱子的上层增量
p95_extra = p95_fct - avg_fct

# 柱子、折线和横坐标标签使用相同的中心位置
x_bar = x
x_line = x

# =========================
# 2) 全局绘图风格
# =========================
plt.rcParams['font.family'] = 'Times New Roman'
plt.rcParams['mathtext.fontset'] = 'stix'
plt.rcParams['font.size'] = 40
plt.rcParams['axes.linewidth'] = 1.8
plt.rcParams['xtick.major.width'] = 1.4
plt.rcParams['ytick.major.width'] = 1.4
plt.rcParams['xtick.direction'] = 'in'
plt.rcParams['ytick.direction'] = 'in'

# 条纹粗细
plt.rcParams['hatch.linewidth'] = 1

# PDF字体嵌入
plt.rcParams['pdf.fonttype'] = 42
plt.rcParams['ps.fonttype'] = 42

# =========================
# 3) 创建画布
# =========================
fig, ax1 = plt.subplots(
    figsize=(10.2, 7.2),
    dpi=300
)

fig.patch.set_facecolor('white')
ax1.set_facecolor('white')
ax1.grid(False)

# 右侧丢包坐标轴
ax2 = ax1.twinx()

# =========================
# 4) 左轴：FCT对数柱状图
# =========================
bar_width = 0.44
avg_color = '#dbe6f7'
p95_color = '#6f92d6'

# 对数坐标不能从0开始
fct_base = 1.0

if np.any(avg_fct <= fct_base):
    raise ValueError(
        f'使用log坐标时，所有Avg FCT必须大于柱状图基准值{fct_base}。'
    )

# Avg FCT：底层，叉形条纹
bars_avg = ax1.bar(
    x_bar,
    avg_fct - fct_base,
    width=bar_width,
    bottom=fct_base,
    color=avg_color,
    alpha=1.0,
    edgecolor='black',
    linewidth=0.9,
    hatch='x',
    label='Avg FCT',
    zorder=2
)

# 95th FCT：上层，单向斜杠
bars_p95 = ax1.bar(
    x_bar,
    p95_extra,
    width=bar_width,
    bottom=avg_fct,
    color=p95_color,
    alpha=1.0,
    edgecolor='black',
    linewidth=0.9,
    hatch='/',
    label='95th FCT',
    zorder=3
)

# FCT使用log坐标
ax1.set_yscale('log')
ax1.set_ylim(1, 20)

# 手动设置对数轴主刻度
ax1.set_yticks([1, 2, 5, 10, 20])
ax1.set_yticklabels(['1', '2', '5', '10', '20'])

# 关闭log轴自动生成的次刻度，防止刻度过密
ax1.yaxis.set_minor_locator(NullLocator())

ax1.set_ylabel(
    'FCT (ms)',
    fontsize=36,
    color='black'
)

ax1.tick_params(
    axis='y',
    labelsize=26,
    colors='black'
)

# =========================
# 5) 右轴：Packet Loss折线图
# =========================
line_loss, = ax2.plot(
    x_line,
    packet_loss,
    color='red',
    marker='o',
    markersize=10,
    markerfacecolor='red',
    markeredgecolor='red',
    markeredgewidth=1.2,
    linewidth=2.8,
    label='Packet Loss',
    zorder=20
)

ax2.set_ylabel(
    r'#of Packet Loss ($\times 10^3$)',
    fontsize=36,
    color='black'
)

ax1.tick_params(
    axis='x',
    labelsize=30,
    colors='black',
    pad=10
)

ax2.set_ylim(0, 70)

# =========================
# 6) 横坐标
# =========================
ax1.set_xticks(x)

ax1.set_xticklabels(
    schemes,
    fontsize=36,                 # 放大横坐标字体
    fontname='Times New Roman',
    color='black',
    rotation=20,                 # 标签倾斜20度
    ha='right',                  # 旋转后右对齐
    rotation_mode='anchor'
)

ax1.tick_params(
    axis='x',
    labelsize=30,
    colors='black',
    pad=8
)


# 左右两端留白
ax1.set_xlim(
    x[0] - 0.65,
    x[-1] + 0.65
)

# =========================
# 7) 图层顺序
# =========================
# ax2必须在ax1上面，保证红线覆盖柱子
ax1.set_zorder(2)
ax2.set_zorder(3)

# ax2背景透明，否则会遮住柱状图
ax2.patch.set_visible(False)

# =========================
# 8) 图例
# =========================
handles = [
    line_loss,
    bars_avg,
    bars_p95
]

labels = [
    'Packet Loss',
    'Avg FCT',
    '95th FCT'
]

# 图例放在上层ax2，避免被ax2曲线覆盖
legend = ax2.legend(
    handles,
    labels,
    loc='upper right',
    frameon=False,       # 删除图例外框
    fontsize=30,         # 放大图例字体
    handlelength=1.6,
    handletextpad=0.5,
    labelspacing=0.5
)

legend.set_zorder(30)

# =========================
# 9) 边框
# =========================
for spine in ax1.spines.values():
    spine.set_linewidth(1.6)

for spine in ax2.spines.values():
    spine.set_linewidth(1.6)

# =========================
# 10) 页面布局
# =========================
plt.subplots_adjust(
    left=0.14,
    right=0.88,
    top=0.96,
    bottom=0.19
)

# =========================
# 11) 保存
# =========================
plt.savefig(
    'combined_metrics_overall_printable.png',
    dpi=300,
    bbox_inches='tight'
)

plt.savefig(
    'combined_metrics_overall_printable.pdf',
    bbox_inches='tight'
)

plt.show()