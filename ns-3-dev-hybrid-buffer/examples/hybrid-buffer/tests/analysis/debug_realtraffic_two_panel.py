#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
从 ns-3 FBM/BMS 的 DebugFBM 日志中提取指定端口数据，
绘制一张包含左右两个子图的大图。

左图：
    - R_D：黑色，最下层
    - Actual Arriving Rate：绿色，第二层
    - Packet Loss：红色，最上层（使用右侧纵轴）

右图：
    - R_S：橙黄色
    - Q_i^S：蓝色

图例：
    - 左图图例放在左图坐标轴框外的正上方，3项排成一行
    - 右图图例放在右图坐标轴框外的正上方，2项排成一行

位置参数：
    参数1：算法，BMS 或 FBM
    参数2：测试用例，例如 tc2-09
    参数3：端口号，例如 4
    参数4：BMS 阈值，仅 BMS 需要，例如 0.2M

使用示例：
    python3 debug_realtraffic_two_panel.py FBM tc2-09 4
    python3 debug_realtraffic_two_panel.py BMS tc2-09 4 0.2M

    python3 debug_realtraffic_two_panel.py FBM tc2-03-h 1

指定输出：
    python3 debug_realtraffic_two_panel.py FBM tc2-09 4 \
        -o FBM_tc2-09_port4.pdf --rate-unit Gbps

    
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import matplotlib
import matplotlib.pyplot as plt
import pandas as pd


# ============================================================
# 1. 全局绘图参数
# ============================================================

# PDF、PS使用TrueType字体，便于论文中编辑
matplotlib.rcParams["pdf.fonttype"] = 42
matplotlib.rcParams["ps.fonttype"] = 42

# 全部使用Times New Roman
plt.rcParams["font.family"] = "Times New Roman"

# 数学公式使用Times New Roman
plt.rcParams["mathtext.fontset"] = "custom"
plt.rcParams["mathtext.rm"] = "Times New Roman"
plt.rcParams["mathtext.it"] = "Times New Roman:italic"
plt.rcParams["mathtext.bf"] = "Times New Roman:bold"

plt.rcParams["axes.unicode_minus"] = False
plt.rcParams["axes.linewidth"] = 1.5

AXIS_FONT_SIZE = 46
TICK_FONT_SIZE = 40
LEGEND_FONT_SIZE = 42

LINE_WIDTH = 2.2
MARKER_SIZE = 8.0

# 每0.5 ms统计一次平均值。日志时间单位为ns。
SAMPLE_INTERVAL_NS = 500_000

# 按用户要求：左图改为黑/绿/红层次，右图通过线型和marker增强区分
DR_COLOR = "#000000"           # R_D：黑色
ACTUAL_RATE_COLOR = "green"  # Actual Arriving Rate：绿色
DROP_COLOR = "red"         # Packet Loss：红色
SR_COLOR = "#1F4E79"           # R_S：蓝色
QIS_COLOR = "#A9CCE3"          # Q_i^S：浅蓝色，不透明   #A9CCE3

# ============================================================
# 2. 日志解析正则
# ============================================================

NUMBER = (
    r"[-+]?"
    r"(?:\d+(?:\.\d*)?|\.\d+)"
    r"(?:[eE][-+]?\d+)?"
)

HEADER_RE = re.compile(
    rf"DebugFBM:\s+time:\s*(?P<time>{NUMBER})"
    rf"\s+port:\s*(?P<port>\d+)"
)

BUFFER_RE = re.compile(
    rf"\bQi:\s*(?P<Qi>{NUMBER})"
    rf"\s+QiS:\s*(?P<QiS>{NUMBER})"
    rf"\s+QiD:\s*(?P<QiD>{NUMBER})"
    rf".*?\bSr:\s*(?P<Sr>{NUMBER})"
    rf"\s+Dr:\s*(?P<Dr>{NUMBER})"
    rf"\s+DT:\s*(?P<DT>{NUMBER})"
)

RATE_RE = re.compile(
    rf"\barrivalRateActual:\s*(?P<actualRate>{NUMBER})"
    rf"\s+ewmaRate:\s*(?P<ewmaRate>{NUMBER})"
)

DROP_RE = re.compile(
    rf"\bdrop_real/total_arrival:\s*"
    rf"(?P<drop_real>{NUMBER})"
    rf"\s*/\s*"
    rf"(?P<total_arrival>{NUMBER})"
)

# 绘图需要的字段
REQUIRED_FIELDS = {
    "time_ns",
    "port",
    "QiS",
    "Sr",
    "Dr",
    "actualRate",
    "drop_real",
}


# ============================================================
# 3. 解析日志
# ============================================================

def parse_log(log_path: Path) -> pd.DataFrame:
    """
    将每个完整的DebugFBM日志块解析为一条记录。
    """

    records: List[Dict[str, float]] = []
    current: Optional[Dict[str, float]] = None

    def save_current() -> None:
        """
        保存当前完整记录。
        """

        nonlocal current

        if (
            current is not None
            and REQUIRED_FIELDS.issubset(current)
        ):
            records.append(current)

        current = None

    with log_path.open(
        "r",
        encoding="utf-8",
        errors="replace",
    ) as file:

        for line_number, line in enumerate(
            file,
            start=1,
        ):
            header_match = HEADER_RE.search(line)

            if header_match:
                save_current()

                current = {
                    "time_ns": float(
                        header_match.group("time")
                    ),
                    "port": int(
                        header_match.group("port")
                    ),
                    "line_number": line_number,
                }

                continue

            if current is None:
                continue

            buffer_match = BUFFER_RE.search(line)

            if buffer_match:
                for name in (
                    "Qi",
                    "QiS",
                    "QiD",
                    "Sr",
                    "Dr",
                    "DT",
                ):
                    current[name] = float(
                        buffer_match.group(name)
                    )

                continue

            rate_match = RATE_RE.search(line)

            if rate_match:
                current["actualRate"] = float(
                    rate_match.group("actualRate")
                )

                current["ewmaRate"] = float(
                    rate_match.group("ewmaRate")
                )

                continue

            drop_match = DROP_RE.search(line)

            if drop_match:
                current["drop_real"] = float(
                    drop_match.group("drop_real")
                )

                current["total_arrival"] = float(
                    drop_match.group("total_arrival")
                )

    # 保存最后一个日志块
    save_current()

    if not records:
        raise ValueError(
            "没有解析到完整记录。请确认DebugFBM块中包含"
            "BufferStates、RateStates和"
            "drop_real/total_arrival。"
        )

    return pd.DataFrame.from_records(records)


# ============================================================
# 4. 单位转换和坐标轴样式
# ============================================================

def convert_time(
    time_ns: pd.Series,
    unit: str,
) -> Tuple[pd.Series, str]:
    """
    将日志中的ns转换为指定横坐标单位。
    """

    factors = {
        "ns": (
            1.0,
            "Time (ns)",
        ),
        "us": (
            1e3,
            "Time (μs)",
        ),
        "ms": (
            1e6,
            "Time (ms)",
        ),
        "s": (
            1e9,
            "Time (s)",
        ),
    }

    divisor, label = factors[unit]

    return (
        time_ns / divisor,
        label,
    )


def set_axis_style(axis: plt.Axes) -> None:
    """
    设置统一的坐标轴样式。
    """

    axis.grid(
        True,
        linestyle="--",
        linewidth=0.65,
        alpha=0.45,
        zorder=0,
    )

    axis.tick_params(
        axis="both",
        which="major",
        labelsize=TICK_FONT_SIZE,
        direction="in",
        width=1.2,
        length=6,
    )

    for spine in axis.spines.values():
        spine.set_linewidth(1.5)

    # 明确设置横坐标刻度字体
    for label in axis.get_xticklabels():
        label.set_fontname(
            "Times New Roman"
        )

    # 明确设置纵坐标刻度字体
    for label in axis.get_yticklabels():
        label.set_fontname(
            "Times New Roman"
        )


# ============================================================
# 5. 绘图
# ============================================================

def plot_port(
    df: pd.DataFrame,
    port: int,
    output_path: Path,
    time_unit: str,
    rate_divisor: float,
    rate_unit: str,
    show: bool,
    dpi: int,
) -> None:
    """
    为指定端口绘制一行两列的大图。
    """

    port_df = df[
        df["port"] == port
    ].copy()

    if port_df.empty:
        available_ports = sorted(
            int(value)
            for value in df["port"].unique()
        )

        raise ValueError(
            f"日志中没有端口 {port} 的完整记录。"
            f"可用端口为：{available_ports}"
        )

    port_df.sort_values(
        [
            "time_ns",
            "line_number",
        ],
        inplace=True,
    )

    port_df.reset_index(
        drop=True,
        inplace=True,
    )

    # 保留一份原始瞬时数据：
    #   Actual Arriving Rate、Packet Loss、QiS直接使用原始值；
    #   仅R_D和R_S每0.5 ms取一次平均值。
    raw_df = port_df.copy()
    original_count = len(raw_df)

    buffer_df = raw_df.copy()
    buffer_df["sample_bin"] = (
        buffer_df["time_ns"] // SAMPLE_INTERVAL_NS
    ).astype(int)

    buffer_df = (
        buffer_df.groupby(
            "sample_bin",
            as_index=False,
        )
        .agg(
            time_ns=("time_ns", "mean"),
            Dr=("Dr", "mean"),
            Sr=("Sr", "mean"),
        )
    )

    raw_df["time"], time_label = convert_time(
        raw_df["time_ns"],
        time_unit,
    )

    buffer_df["time"], _ = convert_time(
        buffer_df["time_ns"],
        time_unit,
    )

    # R_D与Actual Arriving Rate使用相同的缩放规则。
    buffer_df["Dr"] = (
        buffer_df["Dr"] / rate_divisor
    )
    raw_df["actualRate"] = (
        raw_df["actualRate"] / rate_divisor
    )

    # 瞬时曲线保留全部原始点；仅减少marker显示频率，避免符号重叠。
    # 曲线本身没有降采样。
    raw_marker_step = max(
        1,
        len(raw_df) // 250,
    )
    buffer_marker_step = 1

    print(
        f"混合绘图：R_D/R_S由{original_count}个原始点"
        f"聚合为{len(buffer_df)}个0.5 ms平均点；"
        "速率、丢包和QiS使用全部原始瞬时值。"
    )

    fig, (
        ax_left,
        ax_right,
    ) = plt.subplots(
        1,
        2,
        figsize=(18.0, 11),
        dpi=dpi,
        sharex=True,
    )

    fig.patch.set_facecolor(
        "white"
    )

    # ========================================================
    # 左图：R_D、Actual Arriving Rate、Packet Loss
    # ========================================================

    ax_rate = ax_left
    ax_drop = ax_rate.twinx()

    # R_D：黑色，使用左侧纵轴
    line_dr, = ax_rate.plot(
        buffer_df["time"],
        buffer_df["Dr"],
        color=DR_COLOR,
        marker="X",
        linestyle="-",
        linewidth=2.8,
        markersize=MARKER_SIZE,
        markerfacecolor=DR_COLOR,
        markeredgewidth=1.8,
        alpha=0.90,
        markevery=buffer_marker_step,
        zorder=1,
    )

    # Actual Arriving Rate：绿色辅助线，使用左侧纵轴
    line_actual, = ax_rate.plot(
        raw_df["time"],
        raw_df["actualRate"],
        color=ACTUAL_RATE_COLOR,
        linestyle="-",
        linewidth=1.4,
        alpha=0.60,
        zorder=3,
    )

    # Packet Loss：红色，使用右侧纵轴
    line_drop, = ax_drop.plot(
        raw_df["time"],
        raw_df["drop_real"],
        color=DROP_COLOR,
        marker="^",
        linestyle="-",
        linewidth=LINE_WIDTH + 0.6,
        markersize=MARKER_SIZE + 0.6,
        markerfacecolor=DROP_COLOR,
        markeredgewidth=1.6,
        markevery=raw_marker_step,
        zorder=20,
    )

    ax_rate.set_xlabel(
        time_label,
        fontsize=AXIS_FONT_SIZE,
        fontname="Times New Roman",
    )

    ax_rate.set_ylabel(
        f"Rate / Bandwidth ({rate_unit})",
        fontsize=AXIS_FONT_SIZE,
        fontname="Times New Roman",
    )

    ax_drop.set_ylabel(
        "#of Packet Loss",
        fontsize=AXIS_FONT_SIZE,
        fontname="Times New Roman",
    )

    # 左侧速率轴
    ax_rate.set_ylim(
        0,
        1000,
    )

    # 右侧丢包轴
    ax_drop.set_ylim(
        0,
        250,
    )

    set_axis_style(
        ax_rate
    )

    ax_drop.tick_params(
        axis="y",
        which="major",
        labelsize=TICK_FONT_SIZE,
        direction="in",
        width=1.2,
        length=6,
    )

    for label in ax_drop.get_yticklabels():
        label.set_fontname(
            "Times New Roman"
        )

    for spine in ax_drop.spines.values():
        spine.set_linewidth(1.5)

    # 让丢包曲线位于最上层，同时保持背景透明
    ax_rate.set_zorder(1)
    ax_drop.set_zorder(2)
    ax_drop.patch.set_visible(False)

    left_handles = [
        line_dr,
        line_actual,
        line_drop,
    ]

    left_labels = [
        r"$\mathrm{R}_{\mathrm{D}}$",
        "Actual Arriving Rate",
        "#of Packet Loss",
    ]


    # ========================================================
    # 右图：R_S、Q_i^S
    # ========================================================

    # R_S：蓝色实线 + 菱形 marker
    line_sr, = ax_right.plot(
        buffer_df["time"],
        buffer_df["Sr"],
        color=SR_COLOR,
        marker="D",
        linestyle="-",
        linewidth=2.8,
        markersize=MARKER_SIZE + 0.5,
        markerfacecolor=SR_COLOR,
        markeredgewidth=1.8,
        markevery=buffer_marker_step,
        zorder=2,
    )

    # Q_i^S：浅蓝色虚线 + 空心圆 marker
    line_qis, = ax_right.plot(
        raw_df["time"],
        raw_df["QiS"],
        color=QIS_COLOR,
        marker="*",
        linestyle="-",
        linewidth=LINE_WIDTH - 0.2,
        markersize=MARKER_SIZE + 0.8,
        #markerfacecolor="white",
        markeredgecolor=QIS_COLOR,
        markeredgewidth=2.0,
        markevery=raw_marker_step,
        zorder=5,
    )

    ax_right.set_xlabel(
        time_label,
        fontsize=AXIS_FONT_SIZE,
        fontname="Times New Roman",
    )

    ax_right.set_ylabel(
        "Buffer Size (MB)",
        fontsize=AXIS_FONT_SIZE,
        fontname="Times New Roman",
        labelpad=20, 
    )
    # 将纵轴标题放到右边
    ax_right.yaxis.set_label_position("right")

    # 将纵轴刻度和刻度数字也放到右边
    ax_right.yaxis.tick_right()

    # 右侧显示刻度，左侧不显示
    ax_right.tick_params(
        axis="y",
        right=True,
        labelright=True,
        left=False,
        labelleft=False,
    )
    ax_right.set_ylim(
        bottom=0
    )

    set_axis_style(
        ax_right
    )


    # ========================================================
    # 横坐标范围
    # ========================================================
    # 固定横坐标范围为 0～30 ms
    ax_rate.set_xlim(0, 30)

    # 每隔 5 ms 显示一个刻度
    ax_rate.set_xticks([0, 5, 10, 15, 20, 25, 30])

    # 避免两个子图右下角的横轴“30”与右侧纵轴“0”重叠：
    # 将最右侧横坐标刻度标签向左展开，使其右边缘对齐30 ms位置。
    for axis in (ax_rate, ax_right):
        x_tick_labels = axis.get_xticklabels()
        if x_tick_labels:
            x_tick_labels[-1].set_horizontalalignment("right")
    # ========================================================
    # 合并左右子图图例，放在整张图最上方，一行显示
    # ========================================================
    all_handles = [
        line_dr,
        line_actual,
        line_drop,
        line_sr,
        line_qis,
    ]

    all_labels = [
        r"$\mathrm{R}_{\mathrm{D}}$",
        "Actual Arriving Rate",
        "Packet Loss",
        r"$\mathrm{R}_{\mathrm{S}}$",
        r"$\mathrm{Q}_{i}^{\mathrm{S}}$",
    ]

    fig.legend(
        handles=all_handles,
        labels=all_labels,
        loc="upper center",
        bbox_to_anchor=(0.5, 0.98),
        ncol=5,                  # 一行放5个
        frameon=False,
        handlelength=1.7,
        handletextpad=0.45,
        columnspacing=1.0,
        labelspacing=0.0,
        prop={
            "family": "Times New Roman",
            "size": LEGEND_FONT_SIZE,
        },
    )

    # 给顶部总图例保留空间
    fig.subplots_adjust(
        left=0.09,
        right=0.95,
        bottom=0.18,
        top=0.83,
        wspace=0.28,
    )
    # ========================================================
    # 保存图片
    # ========================================================

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    fig.savefig(
        output_path,
        dpi=dpi,
        bbox_inches="tight",
        pad_inches=0.05,
    )

    print(
        f"已保存图片：{output_path}"
    )

    print(
        f"端口 {port}：瞬时曲线绘制{len(raw_df)}个原始点，"
        f"R_D/R_S绘制{len(buffer_df)}个平均点。"
    )

    if show:
        plt.show()
    else:
        plt.close(fig)


# ============================================================
# 6. 输入输出路径
# ============================================================

def resolve_log_path(
    algorithm: str,
    test_case: str,
    threshold: Optional[str],
    data_root: Path,
) -> Path:
    """
    根据算法、用例和阈值构造日志路径。

    FBM：
        data/pbs/<用例>/hybrid-buffer-test-<用例>.txt

    BMS：
        data/BMS/<用例>/<阈值>/
        hybrid-buffer-test-<用例>.txt
    """

    filename = (
        f"hybrid-buffer-test-"
        f"{test_case}.txt"
    )

    if algorithm == "FBM":
        return (
            data_root
            / "pbs"
            / test_case
            / filename
        )

    if threshold is None:
        raise ValueError(
            "BMS算法必须指定阈值，"
            "例如0.2M。"
        )

    return (
        data_root
        / "BMS"
        / test_case
        / threshold
        / filename
    )


def default_output_path(
    algorithm: str,
    test_case: str,
    port: int,
    threshold: Optional[str],
) -> Path:
    """
    生成默认输出图片名。
    """

    parts = [
        algorithm,
        test_case,
    ]

    if threshold is not None:
        parts.append(
            threshold
        )

    parts.extend(
        [
            f"port-{port}",
            "two-panel",
        ]
    )

    return Path(
        "_".join(parts) + ".pdf"
    )


# ============================================================
# 7. 命令行参数
# ============================================================

def build_parser() -> argparse.ArgumentParser:
    """
    构造命令行参数解析器。
    """

    parser = argparse.ArgumentParser(
        description=(
            "解析FBM/BMS DebugFBM日志并绘制"
            "一行两列大图："
            "左图为R_D、Actual Arriving Rate"
            "和Packet Loss，"
            "右图为R_S和Q_i^S。"
        )
    )

    parser.add_argument(
        "algorithm",
        type=lambda value: value.upper(),
        choices=(
            "BMS",
            "FBM",
        ),
        help="算法名称：BMS或FBM",
    )

    parser.add_argument(
        "test_case",
        help=(
            "测试用例，例如tc2-09"
        ),
    )

    parser.add_argument(
        "port",
        type=int,
        help=(
            "需要统计的端口号，例如4"
        ),
    )

    parser.add_argument(
        "threshold",
        nargs="?",
        default=None,
        help=(
            "BMS阈值，例如0.2M；"
            "FBM不需要"
        ),
    )

    parser.add_argument(
        "--data-root",
        type=Path,
        default=None,
        help=(
            "data目录路径。默认使用脚本"
            "所在目录的../data"
        ),
    )

    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help=(
            "输出图片路径。默认生成带算法、"
            "用例和端口号的PDF"
        ),
    )

    parser.add_argument(
        "--time-unit",
        choices=(
            "ns",
            "us",
            "ms",
            "s",
        ),
        default="ms",
        help=(
            "横坐标时间单位，默认ms"
        ),
    )

    parser.add_argument(
        "--rate-divisor",
        type=float,
        default=1.0,
        help=(
            "速率字段缩放除数。"
            "日志已是Gbps时保持1；"
            "日志是bit/s且希望显示Gbps时"
            "填写1e9"
        ),
    )

    parser.add_argument(
        "--rate-unit",
        default="Gbps",
        help=(
            "速率轴单位名称，默认Gbps"
        ),
    )

    parser.add_argument(
        "--dpi",
        type=int,
        default=300,
        help=(
            "输出图片DPI，默认300"
        ),
    )

    parser.add_argument(
        "--show",
        action="store_true",
        help=(
            "保存后弹出图形窗口"
        ),
    )

    return parser


# ============================================================
# 8. 主函数
# ============================================================

def main() -> int:
    """
    主函数。
    """

    parser = build_parser()
    args = parser.parse_args()

    if args.port < 0:
        parser.error(
            "端口号不能为负数。"
        )

    if (
        args.algorithm == "BMS"
        and args.threshold is None
    ):
        parser.error(
            "BMS必须指定阈值，例如："
            "python3 debug_realtraffic_two_panel.py "
            "BMS tc2-09 4 0.2M"
        )

    if (
        args.algorithm == "FBM"
        and args.threshold is not None
    ):
        parser.error(
            "FBM不需要阈值，例如："
            "python3 debug_realtraffic_two_panel.py "
            "FBM tc2-09 4"
        )

    if args.rate_divisor <= 0:
        parser.error(
            "--rate-divisor必须大于0。"
        )

    script_dir = (
        Path(__file__)
        .resolve()
        .parent
    )

    if args.data_root is None:
        data_root = (
            script_dir
            / "../data"
        ).resolve()

    else:
        data_root = (
            args.data_root
            .expanduser()
        )

        if not data_root.is_absolute():
            data_root = (
                Path.cwd()
                / data_root
            ).resolve()

    try:
        log_path = resolve_log_path(
            algorithm=args.algorithm,
            test_case=args.test_case,
            threshold=args.threshold,
            data_root=data_root,
        )

    except ValueError as exc:
        parser.error(
            str(exc)
        )

    if not log_path.is_file():
        print(
            "错误：根据参数生成的日志文件不存在：",
            file=sys.stderr,
        )

        print(
            f"  {log_path}",
            file=sys.stderr,
        )

        print(
            "",
            file=sys.stderr,
        )

        print(
            "路径规则：",
            file=sys.stderr,
        )

        print(
            "  FBM: data/pbs/<用例>/"
            "hybrid-buffer-test-<用例>.txt",
            file=sys.stderr,
        )

        print(
            "  BMS: data/BMS/<用例>/<阈值>/"
            "hybrid-buffer-test-<用例>.txt",
            file=sys.stderr,
        )

        return 1

    output_path = (
        args.output
        or default_output_path(
            algorithm=args.algorithm,
            test_case=args.test_case,
            port=args.port,
            threshold=args.threshold,
        )
    )

    print(
        f"算法：{args.algorithm}"
    )

    print(
        f"用例：{args.test_case}"
    )

    print(
        f"端口：{args.port}"
    )

    if args.threshold is not None:
        print(
            f"BMS阈值：{args.threshold}"
        )

    print(
        f"日志文件：{log_path}"
    )

    try:
        dataframe = parse_log(
            log_path
        )

        plot_port(
            df=dataframe,
            port=args.port,
            output_path=output_path,
            time_unit=args.time_unit,
            rate_divisor=args.rate_divisor,
            rate_unit=args.rate_unit,
            show=args.show,
            dpi=args.dpi,
        )

    except (
        OSError,
        ValueError,
    ) as exc:
        print(
            f"错误：{exc}",
            file=sys.stderr,
        )

        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(
        main()
    )