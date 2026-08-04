#!/usr/bin/env python3
# coding: utf-8
# python3 plot_period_utility.py /home/sj/FBM1/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/pbs/tc2-05/hybrid-buffer-test-tc2-05.txt --ports  0 1

import argparse
import math
import os
import re
from collections import defaultdict

import matplotlib
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.ticker import MaxNLocator, MultipleLocator


# ============================================================
# 1. 全局绘图风格
# ============================================================

# PDF、PS保存为TrueType字体，避免字体转成不可编辑的路径
matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

# ============================================================
# 全部文字统一使用 Times New Roman
# ============================================================

# 普通文本、坐标标题、刻度和图例
plt.rcParams['font.family'] = 'serif'
plt.rcParams['font.serif'] = ['Times New Roman']

# 数学公式也使用 Times New Roman
plt.rcParams['mathtext.fontset'] = 'custom'
plt.rcParams['mathtext.rm'] = 'Times New Roman'
plt.rcParams['mathtext.it'] = 'Times New Roman:italic'
plt.rcParams['mathtext.bf'] = 'Times New Roman:bold'
plt.rcParams['mathtext.cal'] = 'Times New Roman'
plt.rcParams['mathtext.sf'] = 'Times New Roman'

# 避免负号显示成方框
plt.rcParams['axes.unicode_minus'] = False

# 坐标轴边框宽度
plt.rcParams['axes.linewidth'] = 1.5


FINAL_FONT_SIZE = 36
LINE_WIDTH = 2.0
MARKER_SIZE = 5


# 与原 ploting-single.py 保持一致
T_COLOR = 'k'
USRAM_COLOR = 'g'
UDRAM_COLOR = 'r'


# port 0正常显示的时间范围
# PORT0_XLIM = (0.0, 1.6)


# port 1只显示以下两个时间窗口
PORT1_XLIMS = (
    (0.2116, 0.2300),
    (1.4016, 1.4280)
)


# ============================================================
# 2. 正则表达式
# ============================================================

NUMBER_PATTERN = (
    r'[+-]?(?:\d+(?:\.\d*)?|\.\d+)'
    r'(?:[eE][+-]?\d+)?'
)


MIDDLE_VALUE_PATTERN = re.compile(
    rf'^\s*(?P<time>{NUMBER_PATTERN})'
    rf'\s+middle_value_for_plot:'
    rf'.*?port:\s*(?P<port>\d+)'
    rf'.*?newT\[T\+1\]\(ns\):\s*(?P<T>{NUMBER_PATTERN})'
    rf'.*?Usram:\s*(?P<Usram>{NUMBER_PATTERN})'
    rf'.*?Udram:\s*(?P<Udram>{NUMBER_PATTERN})'
)


# ============================================================
# 3. 读取日志
# ============================================================

def parse_log(log_file):
    """
    读取日志文件中的middle_value_for_plot记录。

    返回格式：

    {
        port_id: [
            (time_ms, period_us, usram, udram),
            ...
        ]
    }
    """

    port_data = defaultdict(list)

    with open(
        log_file,
        'r',
        encoding='utf-8',
        errors='ignore'
    ) as file:

        for line_number, line in enumerate(file, start=1):

            match = MIDDLE_VALUE_PATTERN.search(line)

            if match is None:
                continue

            time_ns = float(match.group('time'))
            port = int(match.group('port'))
            period_ns = float(match.group('T'))
            usram = float(match.group('Usram'))
            udram = float(match.group('Udram'))

            values = [
                time_ns,
                period_ns,
                usram,
                udram
            ]

            # 过滤nan、inf等无效值
            if not all(
                math.isfinite(value)
                for value in values
            ):
                print(
                    f'警告：第 {line_number} 行存在无效数值，'
                    '已经跳过'
                )
                continue

            # 单位转换
            time_ms = time_ns / 1_000_000.0
            period_us = period_ns / 1_000.0

            port_data[port].append(
                (
                    time_ms,
                    period_us,
                    usram,
                    udram
                )
            )

    # 每个端口的数据按照时间排序
    for port in port_data:
        port_data[port].sort(
            key=lambda item: item[0]
        )

    return port_data


# ============================================================
# 4. 辅助函数
# ============================================================

def calculate_limits(
    values,
    pad_ratio=0.06,
    minimum_pad=0.01
):
    """
    根据数据计算纵坐标范围，并在上下留出一定空白。
    """

    if not values:
        return 0.0, 1.0

    value_min = min(values)
    value_max = max(values)
    value_range = value_max - value_min

    padding = max(
        value_range * pad_ratio,
        minimum_pad
    )

    return (
        value_min - padding,
        value_max + padding
    )


def extract_segment(records, left, right):
    """
    提取指定时间窗口内的数据。
    """

    return [
        item
        for item in records
        if left <= item[0] <= right
    ]


def save_figure(fig, port, output_dir):
    """
    同时保存PDF和PNG。
    """

    os.makedirs(
        output_dir,
        exist_ok=True
    )

    pdf_path = os.path.join(
        output_dir,
        f'period-utility-port{port}.pdf'
    )

    png_path = os.path.join(
        output_dir,
        f'period-utility-port{port}.png'
    )

    fig.savefig(
        pdf_path,
        bbox_inches='tight',
        pad_inches=0.05
    )

    fig.savefig(
        png_path,
        dpi=300,
        bbox_inches='tight',
        pad_inches=0.05
    )

    print(f'已保存：{pdf_path}')
    print(f'已保存：{png_path}')


def create_legend_handles():
    """
    创建统一图例，供正常图和截断图共同使用。
    """

    return [
        Line2D(
            [0],
            [0],
            label='T',
            color=T_COLOR,
            linewidth=LINE_WIDTH,
            marker='s',
            markersize=MARKER_SIZE,
            markerfacecolor=T_COLOR,
            markeredgecolor=T_COLOR
        ),

        Line2D(
            [0],
            [0],
            label=r'$\mathrm{U}^{S}$',
            color=USRAM_COLOR,
            linewidth=LINE_WIDTH,
            marker='o',
            markersize=MARKER_SIZE,
            markerfacecolor=USRAM_COLOR,
            markeredgecolor=USRAM_COLOR
        ),

        Line2D(
            [0],
            [0],
            label=r'$\mathrm{U}^{D}$',
            color=UDRAM_COLOR,
            linewidth=LINE_WIDTH,
            marker='^',
            markersize=MARKER_SIZE,
            markerfacecolor=UDRAM_COLOR,
            markeredgecolor=UDRAM_COLOR
        )
    ]


# ============================================================
# 5. 绘制一段数据
# ============================================================

def draw_segment(
    ax_t,
    ax_u,
    records
):
    """
    在给定左轴和右轴上绘制T、Usram、Udram。
    """

    time_ms = [
        item[0]
        for item in records
    ]

    period_us = [
        item[1]
        for item in records
    ]

    usram = [
        item[2]
        for item in records
    ]

    udram = [
        item[3]
        for item in records
    ]

    # T：黑色，左轴
    ax_t.plot(
        time_ms,
        period_us,
        color=T_COLOR,
        linewidth=LINE_WIDTH,
        marker='s',
        markersize=MARKER_SIZE,
        markerfacecolor=T_COLOR,
        markeredgecolor=T_COLOR,
        linestyle='-',
        zorder=4
    )

    # SRAM：绿色，右轴
    ax_u.plot(
        time_ms,
        usram,
        color=USRAM_COLOR,
        linewidth=LINE_WIDTH,
        marker='o',
        markersize=MARKER_SIZE,
        markerfacecolor=USRAM_COLOR,
        markeredgecolor=USRAM_COLOR,
        linestyle='-',
        zorder=3
    )

    # DRAM：红色，右轴
    ax_u.plot(
        time_ms,
        udram,
        color=UDRAM_COLOR,
        linewidth=LINE_WIDTH,
        marker='^',
        markersize=MARKER_SIZE,
        markerfacecolor=UDRAM_COLOR,
        markeredgecolor=UDRAM_COLOR,
        linestyle='-',
        zorder=3
    )


# ============================================================
# 6. port 0：正常绘制
# ============================================================

def plot_port0_normal(
    port,
    records,
    output_dir
):
    """
    正常绘制port 0的数据。
    """

    if not records:
        print(
            f'警告：port {port} 没有可绘制的数据'
        )
        return

    period_us = [
        item[1]
        for item in records
    ]

    usram = [
        item[2]
        for item in records
    ]

    udram = [
        item[3]
        for item in records
    ]

    fig, ax_t = plt.subplots(
        figsize=(8, 7.2),
        dpi=300
    )

    fig.patch.set_facecolor('white')
    ax_t.set_facecolor('white')
    ax_t.grid(False)

    # Utility右轴
    ax_u = ax_t.twinx()

    draw_segment(
        ax_t,
        ax_u,
        records
    )

    # 横坐标范围
    # ax_t.set_xlim(*PORT0_XLIM)

    # 横坐标每0.4ms一个主刻度
    ax_t.xaxis.set_major_locator(
        MultipleLocator(0.4)
    )

    # 计算纵坐标范围
    # T 左纵轴仍然自动计算范围
    period_ylim = calculate_limits(
        period_us,
        minimum_pad=0.1
    )

    ax_t.set_ylim(
        *period_ylim
    )

    # Port 0 的 Utility 右纵轴手动指定范围
    ax_u.set_ylim(
        0.9,
        1.2
    )

    # T 左纵轴刻度自动设置
    ax_t.yaxis.set_major_locator(
        MaxNLocator(
            nbins=5
        )
    )

    # Utility 右纵轴刻度间隔
    ax_u.yaxis.set_major_locator(
        MultipleLocator(0.1)
    )

    # 坐标轴名称
    ax_t.set_xlabel(
        'Time (ms)',
        fontsize=FINAL_FONT_SIZE,
        fontname='Times New Roman'
    )

    ax_t.set_ylabel(
        'T (us)',
        fontsize=FINAL_FONT_SIZE,
        fontname='Times New Roman'
    )

    ax_u.set_ylabel(
        'Utility',
        fontsize=FINAL_FONT_SIZE,
        fontname='Times New Roman'
    )

    # 左轴刻度样式
    ax_t.tick_params(
        axis='both',
        labelsize=FINAL_FONT_SIZE,
        direction='in',
        width=1.1,
        length=6
    )

    # 右轴刻度样式
    ax_u.tick_params(
        axis='y',
        labelsize=FINAL_FONT_SIZE,
        direction='in',
        width=1.1,
        length=6
    )

    # 再次明确指定刻度字体
    for label in ax_t.get_xticklabels():
        label.set_fontname(
            'Times New Roman'
        )

    for label in ax_t.get_yticklabels():
        label.set_fontname(
            'Times New Roman'
        )

    for label in ax_u.get_yticklabels():
        label.set_fontname(
            'Times New Roman'
        )

    # 图例
    legend_handles = create_legend_handles()

    legend = ax_t.legend(
        handles=legend_handles,
        loc='lower center',
        bbox_to_anchor=(0.5, 1),
        ncol=3,
        fontsize=FINAL_FONT_SIZE,
        frameon=False,
        handlelength=1.4,
        columnspacing=0.8,
        handletextpad=0.3,
        prop={
            'family': 'Times New Roman',
            'size': FINAL_FONT_SIZE
        }
    )

    legend.get_frame().set_linewidth(0)

    # 再次明确设置图例字体
    for text in legend.get_texts():
        text.set_fontname(
            'Times New Roman'
        )

    # 边框
    for spine in ax_t.spines.values():
        spine.set_linewidth(1.5)

    for spine in ax_u.spines.values():
        spine.set_linewidth(1.5)

    fig.subplots_adjust(
        left=0.17,
        right=0.83,
        bottom=0.16,
        top=0.78
    )

    save_figure(
        fig,
        port,
        output_dir
    )

    plt.close(fig)

    print(
        f'port {port} 正常绘制，'
        f'数据点数量：{len(records)}'
    )


# ============================================================
# 7. port 1：横坐标截断绘制
# ============================================================

def plot_port1_broken(
    port,
    records,
    output_dir
):
    """
    对port 1使用两个时间窗口绘制横坐标截断图。
    """

    if not records:
        print(
            f'警告：port {port} 没有可绘制的数据'
        )
        return

    # 提取第一个时间窗口
    left_records = extract_segment(
        records,
        PORT1_XLIMS[0][0],
        PORT1_XLIMS[0][1]
    )

    # 提取第二个时间窗口
    right_records = extract_segment(
        records,
        PORT1_XLIMS[1][0],
        PORT1_XLIMS[1][1]
    )

    if not left_records:
        print(
            '警告：port 1第一个截断窗口内没有数据：'
            f'{PORT1_XLIMS[0]}'
        )

    if not right_records:
        print(
            '警告：port 1第二个截断窗口内没有数据：'
            f'{PORT1_XLIMS[1]}'
        )

    visible_records = (
        left_records
        + right_records
    )

    if not visible_records:
        raise RuntimeError(
            'port 1的两个截断时间窗口内都没有数据'
        )

    # 截断图只根据可见数据计算纵坐标范围
    visible_period = [
        item[1]
        for item in visible_records
    ]

    visible_usram = [
        item[2]
        for item in visible_records
    ]

    visible_udram = [
        item[3]
        for item in visible_records
    ]

    period_ylim = calculate_limits(
        visible_period,
        minimum_pad=0.1
    )

    utility_ylim = calculate_limits(
        visible_usram + visible_udram,
        minimum_pad=0.01
    )

    # 根据两个时间窗口的实际长度设置宽度比例
    left_width = (
        PORT1_XLIMS[0][1]
        - PORT1_XLIMS[0][0]
    )

    right_width = (
        PORT1_XLIMS[1][1]
        - PORT1_XLIMS[1][0]
    )

    fig = plt.figure(
        figsize=(8, 8),
        dpi=300
    )

    fig.patch.set_facecolor('white')

    grid = fig.add_gridspec(
        nrows=1,
        ncols=2,
        width_ratios=[
            left_width,
            right_width
        ],
        wspace=0.06
    )

    # 左侧时间窗口
    ax_t_left = fig.add_subplot(
        grid[0, 0]
    )

    # 右侧时间窗口，与左图共享T轴
    ax_t_right = fig.add_subplot(
        grid[0, 1],
        sharey=ax_t_left
    )

    # Utility轴
    ax_u_left = ax_t_left.twinx()
    ax_u_right = ax_t_right.twinx()

    # 绘制两段数据
    # 分别调用，避免跨越中间空白区域连线
    if left_records:
        draw_segment(
            ax_t_left,
            ax_u_left,
            left_records
        )

    if right_records:
        draw_segment(
            ax_t_right,
            ax_u_right,
            right_records
        )

    # 设置两个时间窗口
    ax_t_left.set_xlim(
        *PORT1_XLIMS[0]
    )

    ax_t_right.set_xlim(
        *PORT1_XLIMS[1]
    )

    # 两侧使用相同的T纵坐标范围
    ax_t_left.set_ylim(
        *period_ylim
    )

    ax_t_right.set_ylim(
        *period_ylim
    )

    # 两侧使用相同的Utility纵坐标范围
    ax_u_left.set_ylim(
        *utility_ylim
    )

    ax_u_right.set_ylim(
        *utility_ylim
    )

    # 横坐标每0.01ms一个主刻度
    ax_t_left.xaxis.set_major_locator(
        MultipleLocator(0.01)
    )

    ax_t_right.xaxis.set_major_locator(
        MultipleLocator(0.01)
    )

    # T纵坐标刻度
    ax_t_left.yaxis.set_major_locator(
        MaxNLocator(
            nbins=5
        )
    )

    # Utility纵坐标刻度
    ax_u_right.yaxis.set_major_locator(
        MaxNLocator(
            nbins=5
        )
    )

    # 背景和网格
    for axis in [
        ax_t_left,
        ax_t_right,
        ax_u_left,
        ax_u_right
    ]:
        axis.set_facecolor('white')
        axis.grid(False)

    # 左图显示Period T纵轴和横轴
    ax_t_left.tick_params(
        axis='both',
        labelsize=FINAL_FONT_SIZE,
        direction='in',
        width=1.1,
        length=6
    )

    # 右图显示横轴
    ax_t_right.tick_params(
        axis='x',
        labelsize=FINAL_FONT_SIZE,
        direction='in',
        width=1.1,
        length=6
    )

    # 右图不重复显示左侧T轴刻度
    ax_t_right.tick_params(
        axis='y',
        left=False,
        labelleft=False
    )

    # 左侧Utility轴不显示右侧刻度
    ax_u_left.tick_params(
        axis='y',
        right=False,
        labelright=False
    )

    ax_u_left.spines[
        'right'
    ].set_visible(False)

    # 右侧显示Utility轴
    ax_u_right.tick_params(
        axis='y',
        labelsize=FINAL_FONT_SIZE,
        direction='in',
        width=1.1,
        length=6
    )

    ax_u_right.set_ylabel(
        'Utility',
        fontsize=FINAL_FONT_SIZE,
        fontname='Times New Roman'
    )

    # 明确设置所有可见刻度字体
    for label in ax_t_left.get_xticklabels():
        label.set_fontname(
            'Times New Roman'
        )

    for label in ax_t_left.get_yticklabels():
        label.set_fontname(
            'Times New Roman'
        )

    for label in ax_t_right.get_xticklabels():
        label.set_fontname(
            'Times New Roman'
        )

    for label in ax_u_right.get_yticklabels():
        label.set_fontname(
            'Times New Roman'
        )

    # 隐藏中间相邻边框
    ax_t_left.spines[
        'right'
    ].set_visible(False)

    ax_t_right.spines[
        'left'
    ].set_visible(False)

    ax_u_right.spines[
        'left'
    ].set_visible(False)

    # 公共横坐标名称
    fig.text(
        0.5,
        0.065,
        'Time (ms)',
        ha='center',
        va='center',
        fontsize=FINAL_FONT_SIZE,
        fontname='Times New Roman'
    )

    # 公共左侧纵坐标名称
    fig.text(
        0.035,
        0.47,
        'T (us)',
        ha='center',
        va='center',
        rotation='vertical',
        fontsize=FINAL_FONT_SIZE,
        fontname='Times New Roman'
    )

    # ========================================================
    # 绘制截断符号
    # ========================================================

    break_size = 0.018
    break_linewidth = 1.3

    # 左侧图右边界截断符号的参数
    left_break_kwargs = dict(
        transform=ax_t_left.transAxes,
        color='black',
        clip_on=False,
        linewidth=break_linewidth
    )

    # 左侧图右下角截断符号
    ax_t_left.plot(
        (
            1 - break_size,
            1 + break_size
        ),
        (
            -break_size,
            break_size
        ),
        **left_break_kwargs
    )

    # 左侧图右上角截断符号
    ax_t_left.plot(
        (
            1 - break_size,
            1 + break_size
        ),
        (
            1 - break_size,
            1 + break_size
        ),
        **left_break_kwargs
    )

    # 右侧图左边界截断符号的参数
    right_break_kwargs = dict(
        transform=ax_t_right.transAxes,
        color='black',
        clip_on=False,
        linewidth=break_linewidth
    )

    # 右侧图左下角截断符号
    ax_t_right.plot(
        (
            -break_size,
            break_size
        ),
        (
            -break_size,
            break_size
        ),
        **right_break_kwargs
    )

    # 右侧图左上角截断符号
    ax_t_right.plot(
        (
            -break_size,
            break_size
        ),
        (
            1 - break_size,
            1 + break_size
        ),
        **right_break_kwargs
    )

    # ========================================================
    # 图例
    # ========================================================

    legend_handles = create_legend_handles()

    legend = fig.legend(
        handles=legend_handles,
        loc='upper center',
        bbox_to_anchor=(0.5, 0.97),
        ncol=3,
        fontsize=FINAL_FONT_SIZE,
        frameon=False,
        handlelength=1.4,
        columnspacing=0.8,
        handletextpad=0.3,
        prop={
            'family': 'Times New Roman',
            'size': FINAL_FONT_SIZE
        }
    )

    legend.get_frame().set_linewidth(0)

    # 明确设置图例文字字体
    for text in legend.get_texts():
        text.set_fontname(
            'Times New Roman'
        )

    # 设置边框宽度
    for axis in [
        ax_t_left,
        ax_t_right,
        ax_u_left,
        ax_u_right
    ]:
        for spine in axis.spines.values():
            if spine.get_visible():
                spine.set_linewidth(1.5)

    fig.subplots_adjust(
        left=0.17,
        right=0.83,
        bottom=0.17,
        top=0.78,
        wspace=0.06
    )

    save_figure(
        fig,
        port,
        output_dir
    )

    plt.close(fig)

    print(
        f'port {port} 使用横坐标截断'
    )

    print(
        f'第一个窗口：{PORT1_XLIMS[0]}，'
        f'数据点数量：{len(left_records)}'
    )

    print(
        f'第二个窗口：{PORT1_XLIMS[1]}，'
        f'数据点数量：{len(right_records)}'
    )


# ============================================================
# 8. 根据端口选择绘图方式
# ============================================================

def plot_one_port(
    port,
    records,
    output_dir
):
    """
    port 0：正常绘图。

    port 1：使用两个时间窗口进行截断绘图。
    """

    if port == 0:
        plot_port0_normal(
            port,
            records,
            output_dir
        )

    elif port == 1:
        plot_port1_broken(
            port,
            records,
            output_dir
        )

    else:
        print(
            f'警告：当前只处理port 0和port 1，'
            f'跳过port {port}'
        )


# ============================================================
# 9. 主函数
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description=(
            '从FBM日志中绘制T、Usram和Udram变化曲线；'
            'port 0正常绘制，port 1进行时间轴截断'
        )
    )

    parser.add_argument(
        'log_file',
        help=(
            'hybrid-buffer-test-*.txt日志文件路径'
        )
    )

    parser.add_argument(
        '--ports',
        nargs='+',
        type=int,
        default=[0, 1],
        help=(
            '需要绘制的端口，默认绘制port 0和port 1；'
            'port 0正常绘制，port 1进行截断'
        )
    )

    parser.add_argument(
        '--output-dir',
        default='period-utility-figures',
        help='图片输出目录'
    )

    args = parser.parse_args()

    if not os.path.isfile(
        args.log_file
    ):
        raise FileNotFoundError(
            f'日志文件不存在：{args.log_file}'
        )

    port_data = parse_log(
        args.log_file
    )

    if not port_data:
        raise RuntimeError(
            '没有解析到middle_value_for_plot数据，'
            '请检查日志内容'
        )

    print(
        '日志中解析到的端口：',
        sorted(
            port_data.keys()
        )
    )

    generated_count = 0

    for port in args.ports:

        if port not in port_data:
            print(
                f'警告：日志中没有找到port {port}的'
                ' middle_value_for_plot数据'
            )
            continue

        plot_one_port(
            port,
            port_data[port],
            args.output_dir
        )

        generated_count += 1

    if generated_count == 0:
        raise RuntimeError(
            '指定端口均无数据，没有生成图片'
        )


if __name__ == '__main__':
    main()