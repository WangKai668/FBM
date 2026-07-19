#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
从 ns-3 FBM 调试日志中提取指定端口的数据，并绘制一个 2×2 大图。

位置参数：
    参数1：算法，BMS 或 FBM
    参数2：测试用例，例如 tc2-09
    参数3：端口号，例如 4
    参数4：BMS 阈值，仅 BMS 需要，例如 0.2M

默认以脚本所在目录为基准查找 data 目录：
    FBM:
        data/pbs/<用例>/hybrid-buffer-test-<用例>.txt

    BMS:
        data/BMS/<用例>/<阈值>/hybrid-buffer-test-<用例>.txt

使用示例：

    python3 debug_realtraffic.py FBM tc2-09 4

    python3 debug_realtraffic.py BMS tc2-09 4 0.2M

"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import matplotlib.pyplot as plt
import pandas as pd


NUMBER = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"


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
    rf"(?P<drop_real>{NUMBER})\s*/\s*(?P<total_arrival>{NUMBER})"
)


REQUIRED_FIELDS = {
    "time_ns",
    "port",
    "Qi",
    "QiS",
    "QiD",
    "Sr",
    "DT",
    "Dr",
    "ewmaRate",
    "actualRate",
    "drop_real",
}


def parse_log(log_path: Path) -> pd.DataFrame:
    """解析 FBM 日志，每个完整的 DebugFBM 块形成一条记录。"""
    records: List[Dict[str, float]] = []
    current: Optional[Dict[str, float]] = None

    def save_current() -> None:
        nonlocal current
        if current is not None and REQUIRED_FIELDS.issubset(current):
            records.append(current)
        current = None

    with log_path.open("r", encoding="utf-8", errors="replace") as f:
        for line_number, line in enumerate(f, start=1):
            header_match = HEADER_RE.search(line)
            if header_match:
                save_current()
                current = {
                    "time_ns": float(header_match.group("time")),
                    "port": int(header_match.group("port")),
                    "line_number": line_number,
                }
                continue

            if current is None:
                continue

            buffer_match = BUFFER_RE.search(line)
            if buffer_match:
                for name in ("Qi", "QiS", "QiD", "Sr", "Dr", "DT"):
                    current[name] = float(buffer_match.group(name))
                continue

            rate_match = RATE_RE.search(line)
            if rate_match:
                current["actualRate"] = float(rate_match.group("actualRate"))
                current["ewmaRate"] = float(rate_match.group("ewmaRate"))
                continue

            drop_match = DROP_RE.search(line)
            if drop_match:
                current["drop_real"] = float(drop_match.group("drop_real"))
                current["total_arrival"] = float(drop_match.group("total_arrival"))

    save_current()

    if not records:
        raise ValueError(
            "没有解析到完整记录。请确认日志中每个 DebugFBM 块都包含 "
            "BufferStates、RateStates 以及 drop_real/total_arrival。"
        )

    return pd.DataFrame.from_records(records)


def convert_time(time_ns: pd.Series, unit: str) -> Tuple[pd.Series, str]:
    factors = {
        "ns": (1.0, "Time (ns)"),
        "us": (1e3, "Time (μs)"),
        "ms": (1e6, "Time (ms)"),
        "s": (1e9, "Time (s)"),
    }
    divisor, label = factors[unit]
    return time_ns / divisor, label


def plot_port(
    df: pd.DataFrame,
    port: int,
    output_path: Path,
    time_unit: str,
    rate_divisor: float,
    rate_unit: str,
    title: Optional[str],
    show: bool,
    dpi: int,
) -> None:
    port_df = df[df["port"] == port].copy()

    if port_df.empty:
        available_ports = sorted(int(v) for v in df["port"].unique())
        raise ValueError(
            f"日志中没有端口 {port} 的完整记录。可用端口为：{available_ports}"
        )

    port_df.sort_values(["time_ns", "line_number"], inplace=True)
    port_df.reset_index(drop=True, inplace=True)

    port_df["time"], time_label = convert_time(port_df["time_ns"], time_unit)

    for name in ("Dr", "ewmaRate", "actualRate"):
        port_df[name] = port_df[name] / rate_divisor

    # 数据量较大时减少 marker 数量，但不对曲线数据做降采样。
    marker_step = max(1, len(port_df) // 250)

    fig, axes = plt.subplots(
        2,
        2,
        figsize=(16, 10),
        sharex=True,
        constrained_layout=True,
    )

    ax_q, ax_sram = axes[0]
    ax_rate, ax_drop = axes[1]

    # ============================================================
    # 左上：Qi、QiD
    # ============================================================
    for name in ("Qi", "QiD"):
        ax_q.plot(
            port_df["time"],
            port_df[name],
            marker="o",
            linestyle="-",
            linewidth=1.2,
            markersize=3.0,
            markevery=marker_step,
            label=name,
        )

    ax_q.set_ylabel("Queue Size (MB)")
    ax_q.set_title(f"Port {port}: Qi and QiD")
    ax_q.grid(True, linestyle="--", linewidth=0.6, alpha=0.6)
    ax_q.legend(loc="best")

    # ============================================================
    # 右上：左轴 QiS、Sr；右轴 DT
    # ============================================================
    for name in ("QiS", "Sr"):
        ax_sram.plot(
            port_df["time"],
            port_df[name],
            marker="o",
            linestyle="-",
            linewidth=1.2,
            markersize=3.0,
            markevery=marker_step,
            label=name,
            color="red" if name == "QiS" else "black",  # 将 QiS 设置为红色
        )

    ax_sram.set_ylabel("QiS / Sr (MB)")
    ax_sram.set
    ax_sram.set_title("QiS and Sr (left axis); DT (right axis)")
    ax_sram.grid(True, linestyle="--", linewidth=0.6, alpha=0.6)

    # ax_dt = ax_sram.twinx()
    # ax_dt.plot(
    #     port_df["time"],
    #     port_df["DT"],
    #     marker="s",
    #     linestyle="--",
    #     linewidth=1.2,
    #     markersize=3.0,
    #     markevery=marker_step,
    #     label="DT",
    # )
    # ax_dt.set_ylabel("DT (MB)")

    handles_left, labels_left = ax_sram.get_legend_handles_labels()
    # handles_right, labels_right = ax_dt.get_legend_handles_labels()
    ax_sram.legend(
        handles_left, #+ handles_right,
        labels_left, # + labels_right,
        loc="best",
    )

    # ============================================================
    # 左下：Dr、ewmaRate、actualRate
    # ============================================================
    for name in ("Dr", "ewmaRate", "actualRate"):
        ax_rate.plot(
            port_df["time"],
            port_df[name],
            marker="o",
            linestyle="-",
            linewidth=1.2,
            markersize=3.0,
            markevery=marker_step,
            label=name,
        )

    ax_rate.set_ylim(0,1200)
    ax_rate.set_xlabel(time_label)
    ax_rate.set_ylabel(f"Rate ({rate_unit})")
    ax_rate.set_title("Dr, ewmaRate and actualRate")
    ax_rate.grid(True, linestyle="--", linewidth=0.6, alpha=0.6)
    ax_rate.legend(loc="best")

    # ============================================================
    # 右下：drop_real
    # ============================================================
    ax_drop.plot(
        port_df["time"],
        port_df["drop_real"],
        marker="o",
        linestyle="-",
        linewidth=1.2,
        markersize=3.0,
        markevery=marker_step,
        label="drop_real",
    )

    ax_drop.set_xlabel(time_label)
    ax_drop.set_ylabel("Dropped Packets")
    ax_drop.set_title("drop_real")
    ax_drop.grid(True, linestyle="--", linewidth=0.6, alpha=0.6)
    ax_drop.legend(loc="best")

    fig.suptitle(
        title if title else f"FBM Statistics for Port {port}",
        fontsize=15,
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=dpi, bbox_inches="tight")

    print(f"已保存图片：{output_path}")
    print(f"端口 {port} 共绘制 {len(port_df)} 条完整记录。")

    if show:
        plt.show()
    else:
        plt.close(fig)



def resolve_log_path(
    algorithm: str,
    test_case: str,
    threshold: Optional[str],
    data_root: Path,
) -> Path:
    """
    根据算法、用例和阈值构造日志文件路径。

    当前目录约定：
        FBM 对应 data/pbs/
        BMS 对应 data/BMS/
    """
    filename = f"hybrid-buffer-test-{test_case}.txt"

    if algorithm == "FBM":
        return data_root / "pbs" / test_case / filename

    if threshold is None:
        raise ValueError("BMS 算法必须指定阈值，例如 0.2M。")

    return data_root / "BMS" / test_case / threshold / filename


def default_output_path(
    algorithm: str,
    test_case: str,
    port: int,
    threshold: Optional[str],
) -> Path:
    """生成不容易互相覆盖的默认图片文件名。"""
    parts = [algorithm, test_case]

    if threshold is not None:
        parts.append(threshold)

    parts.append(f"port-{port}")
    return Path("_".join(parts) + ".png")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "根据算法、测试用例和端口号，自动定位相对路径下的 "
            "ns-3 FBM/BMS 调试日志并绘图。"
        )
    )

    parser.add_argument(
        "algorithm",
        type=lambda value: value.upper(),
        choices=("BMS", "FBM"),
        help="算法名称：BMS 或 FBM",
    )
    parser.add_argument(
        "test_case",
        help="测试用例名称，例如 tc2-09",
    )
    parser.add_argument(
        "port",
        type=int,
        help="需要统计的端口号，例如 4",
    )
    parser.add_argument(
        "threshold",
        nargs="?",
        default=None,
        help="BMS 阈值，例如 0.2M；FBM 不需要该参数",
    )
    parser.add_argument(
        "--data-root",
        type=Path,
        default=None,
        help=(
            "data 目录路径。默认使用脚本所在目录下的 data，"
            "即 <脚本目录>/data"
        ),
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help=(
            "输出图片路径。默认按算法、用例、阈值和端口号自动命名"
        ),
    )
    parser.add_argument(
        "--time-unit",
        choices=("ns", "us", "ms", "s"),
        default="ms",
        help="横轴时间单位，默认 ms",
    )
    parser.add_argument(
        "--rate-divisor",
        type=float,
        default=1.0,
        help=(
            "速率字段缩放除数。日志是 bit/s 且希望显示 Gbps 时填 1e9；"
            "日志已经是 Gbps 时保持默认 1"
        ),
    )
    parser.add_argument(
        "--rate-unit",
        default="log units",
        help="速率轴单位名称，例如 Gbps，默认 log units",
    )
    parser.add_argument(
        "--title",
        default=None,
        help="整张图的自定义标题",
    )
    parser.add_argument(
        "--dpi",
        type=int,
        default=200,
        help="输出图片 DPI，默认 200",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="保存后弹出图形窗口",
    )

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.port < 0:
        parser.error("端口号不能为负数。")

    if args.algorithm == "BMS" and args.threshold is None:
        parser.error(
            "BMS 算法必须额外指定阈值，例如："
            "python3 debug_realtraffic.py BMS tc2-09 4 0.2M"
        )

    if args.algorithm == "FBM" and args.threshold is not None:
        parser.error(
            "FBM 不需要阈值。正确示例："
            "python3 debug_realtraffic.py FBM tc2-09 4"
        )

    if args.rate_divisor <= 0:
        parser.error("--rate-divisor 必须大于 0。")

    script_dir = Path(__file__).resolve().parent

    if args.data_root is None:
        data_root = script_dir / "../data"
    else:
        data_root = args.data_root.expanduser()
        if not data_root.is_absolute():
            data_root = (Path.cwd() / data_root).resolve()

    try:
        log_path = resolve_log_path(
            algorithm=args.algorithm,
            test_case=args.test_case,
            threshold=args.threshold,
            data_root=data_root,
        )
    except ValueError as exc:
        parser.error(str(exc))

    if not log_path.is_file():
        print("错误：根据参数生成的日志文件不存在：", file=sys.stderr)
        print(f"  {log_path}", file=sys.stderr)
        print("", file=sys.stderr)
        print("当前路径规则：", file=sys.stderr)
        print(
            "  FBM: data/pbs/<用例>/hybrid-buffer-test-<用例>.txt",
            file=sys.stderr,
        )
        print(
            "  BMS: data/BMS/<用例>/<阈值>/"
            "hybrid-buffer-test-<用例>.txt",
            file=sys.stderr,
        )
        print(
            "如 data 不在脚本目录旁边，请使用 --data-root 指定。",
            file=sys.stderr,
        )
        return 1

    output_path = args.output or default_output_path(
        algorithm=args.algorithm,
        test_case=args.test_case,
        port=args.port,
        threshold=args.threshold,
    )

    auto_title_parts = [
        args.algorithm,
        args.test_case,
        f"Port {args.port}",
    ]
    if args.threshold is not None:
        auto_title_parts.insert(2, args.threshold)

    plot_title = args.title or " | ".join(auto_title_parts)

    print(f"算法：{args.algorithm}")
    print(f"用例：{args.test_case}")
    print(f"端口：{args.port}")
    if args.threshold is not None:
        print(f"BMS 阈值：{args.threshold}")
    print(f"日志文件：{log_path}")

    try:
        df = parse_log(log_path)
        plot_port(
            df=df,
            port=args.port,
            output_path=output_path,
            time_unit=args.time_unit,
            rate_divisor=args.rate_divisor,
            rate_unit=args.rate_unit,
            title=plot_title,
            show=args.show,
            dpi=args.dpi,
        )
    except (OSError, ValueError) as exc:
        print(f"错误：{exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())