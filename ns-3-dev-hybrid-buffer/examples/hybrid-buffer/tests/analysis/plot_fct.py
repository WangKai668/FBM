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
    'FBM'
]

x = np.arange(len(schemes))

avg_fct = np.array([5.790, 4.187, 2.048, 1.985, 1.985, 1.215])
p95_fct = np.array([15.058, 5.265, 5.239, 5.240, 5.240, 5.240])

# 95th 叠在 avg 上面，因此画增量
p95_extra = p95_fct - avg_fct

# =========================
# 2) 全局风格
# =========================
plt.rcParams['font.family'] = 'serif'
plt.rcParams['font.size'] = 16
plt.rcParams['axes.linewidth'] = 1.6
plt.rcParams['xtick.major.width'] = 1.2
plt.rcParams['ytick.major.width'] = 1.2
plt.rcParams['xtick.direction'] = 'in'
plt.rcParams['ytick.direction'] = 'in'

# =========================
# 3) 创建图像
# =========================
fig, ax = plt.subplots(figsize=(8.2, 6.2), dpi=300)
fig.patch.set_facecolor('white')
ax.set_facecolor('white')
ax.grid(False)

bar_width = 0.52

# 配色
p95_color = '#5b84d7'
avg_color = '#c7d7f2'

# =========================
# 4) 柱状图
# =========================
# Avg FCT：底层
bars_avg = ax.bar(
    x,
    avg_fct,
    width=bar_width,
    color=avg_color,
    alpha=0.90,
    edgecolor='#79BBB7',
    linewidth=1.0,
    label='Avg FCT',
    zorder=2
)

# 95th FCT：上层
bars_p95 = ax.bar(
    x,
    p95_extra,
    width=bar_width,
    bottom=avg_fct,
    color=p95_color,
    alpha=0.90,
    edgecolor='#276C69',
    linewidth=1.0,
    label='95th FCT',
    zorder=3
)

# =========================
# 5) 坐标轴设置
# =========================
ax.set_ylabel('FCT (ms)', fontsize=20, color='black')
ax.set_xticks(x)
ax.set_xticklabels(schemes, fontsize=15, color='black', rotation=10, ha='right')
ax.tick_params(axis='x', labelsize=15, colors='black')
ax.tick_params(axis='y', labelsize=16, colors='black')
ax.set_ylim(0, 16.2)

# =========================
# 6) 数据标注
# =========================
# Avg FCT：柱内标注
for xi, yi in zip(x, avg_fct):
    ax.text(
        xi,
        yi * 0.5,
        f'{yi:.3f}',
        ha='center',
        va='center',
        fontsize=10.5,
        color='black'
    )

# 95th FCT：柱顶标注
for xi, yi in zip(x, p95_fct):
    ax.text(
        xi,
        yi + 0.15,
        f'{yi:.3f}',
        ha='center',
        va='bottom',
        fontsize=10.5,
        color='black'
    )

# =========================
# 7) 图例
# =========================
legend = ax.legend(
    loc='upper right',
    frameon=True,
    fontsize=14,
    edgecolor='black',
    fancybox=False
)
legend.get_frame().set_alpha(1.0)

# =========================
# 8) 边框
# =========================
for spine in ax.spines.values():
    spine.set_linewidth(1.4)

# =========================
# 9) 底部标题
# =========================
fig.text(
    0.5, 0.01,
    '(b) Short Flow',
    ha='center',
    va='bottom',
    fontsize=18
)

plt.tight_layout(rect=[0, 0.05, 1, 1])

# =========================
# 10) 保存
# =========================
plt.savefig('short_flow_fct.png', dpi=300, bbox_inches='tight')
plt.savefig('short_flow_fct.pdf', bbox_inches='tight')
plt.show()