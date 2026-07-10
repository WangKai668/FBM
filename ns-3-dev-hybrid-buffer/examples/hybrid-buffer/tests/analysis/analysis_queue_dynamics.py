#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
解析 FBM 当前实际输出的缓存/端口队列日志，并绘制：

1. 当前端口队列长度
2. SRAM 已使用量
3. DRAM 已使用量

当前 C++ 日志是两行一组，例如：

当前端口队列长度: 1480 剩余SRAM缓存: 4192824剩余DRAM缓存: 2147483648
Time:+100496ns  packet:8 端口:0 存入片内

因此不能按 mmu-Pqs、redQueueDisc、PointToPointReorderNetDevice 的单行格式解析。

用法：
    python3 analysis_queue_dynamics_fixed.py hybrid-buffer-test-tc2-05-tcp.txt
    python3 analysis_queue_dynamics_fixed.py queue_output.txt --port 0 --no-show
    python3 analysis_queue_dynamics_fixed.py run.log --per-port --aggregate max
"""

from __future__ import annotations

import argparse
import math
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import DefaultDict, Dict, Iterable, List, Optional, Sequence, Tuple


NUMBER = r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?"

QUEUE_STATE_RE = re.compile(
    rf"当前端口队列长度\s*:\s*(?P<queue>{NUMBER})\s*"
    rf"剩余SRAM缓存\s*:\s*(?P<sram>{NUMBER})\s*"
    rf"剩余DRAM缓存\s*:\s*(?P<dram>{NUMBER})",
    re.IGNORECASE,
)

ACTION_RE = re.compile(
    rf"Time\s*:\s*\+?(?P<time>{NUMBER})\s*"
    rf"(?P<unit>ns|us|µs|ms|s)?"
    rf".*?packet\s*:\s*(?P<packet>\d+)"
    rf".*?端口\s*:\s*(?P<port>\d+)"
    rf"(?P<action>.*)$",
    re.IGNORECASE,
)


@dataclass(frozen=True)
class QueueSample:
    time_s: float
    packet_id: int
    port: int
    action: str
    port_queue_bytes: float
    sram_remaining_bytes: float
    dram_remaining_bytes: float


Point = Tuple[float, float]


def time_to_seconds(value: float, unit: Optional[str]) -> float:
    normalized = (unit or "s").lower()
    factors = {
        "s": 1.0,
        "ms": 1e-3,
        "us": 1e-6,
        "µs": 1e-6,
        "ns": 1e-9,
    }
    if normalized not in factors:
        raise ValueError(f"不支持的时间单位：{unit}")
    return value * factors[normalized]


def normalize_action(text: str) -> str:
    text = text.strip()
    if "存入片外" in text:
        return "存入片外"
    if "存入片内" in text:
        return "存入片内"
    if "丢包" in text or "DROP" in text.upper():
        return "丢包"
    return text or "未知"


def parse_queue_log(path: Path, debug: bool = False) -> List[QueueSample]:
    """
    将“队列状态行”与其后的“Time/packet/端口/决策行”配对。

    队列状态是在当前报文执行缓存决策前打印的，因此图中的值表示
    该报文到达时看到的队列/缓存状态。
    """
    samples: List[QueueSample] = []
    pending_state: Optional[Tuple[float, float, float, int]] = None
    unpaired_states = 0
    unmatched_action_lines = 0

    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line_number, line in enumerate(handle, start=1):
            state_match = QUEUE_STATE_RE.search(line)
            if state_match:
                if pending_state is not None:
                    unpaired_states += 1

                pending_state = (
                    float(state_match.group("queue")),
                    float(state_match.group("sram")),
                    float(state_match.group("dram")),
                    line_number,
                )
                continue

            action_match = ACTION_RE.search(line)
            if action_match and pending_state is not None:
                queue_bytes, sram_remaining, dram_remaining, state_line = pending_state

                sample = QueueSample(
                    time_s=time_to_seconds(
                        float(action_match.group("time")),
                        action_match.group("unit"),
                    ),
                    packet_id=int(action_match.group("packet")),
                    port=int(action_match.group("port")),
                    action=normalize_action(action_match.group("action")),
                    port_queue_bytes=queue_bytes,
                    sram_remaining_bytes=sram_remaining,
                    dram_remaining_bytes=dram_remaining,
                )

                if (
                    math.isfinite(sample.time_s)
                    and math.isfinite(sample.port_queue_bytes)
                    and math.isfinite(sample.sram_remaining_bytes)
                    and math.isfinite(sample.dram_remaining_bytes)
                    and sample.port_queue_bytes >= 0
                    and sample.sram_remaining_bytes >= 0
                    and sample.dram_remaining_bytes >= 0
                ):
                    samples.append(sample)
                elif debug:
                    print(
                        f"警告：第 {state_line}/{line_number} 行包含非法数值，已跳过。",
                        file=sys.stderr,
                    )

                pending_state = None
                continue

            if action_match and pending_state is None:
                unmatched_action_lines += 1
                if debug and unmatched_action_lines <= 10:
                    print(
                        f"警告：第 {line_number} 行存在 Time/packet/端口，"
                        f"但前面没有可配对的队列状态：{line.strip()}",
                        file=sys.stderr,
                    )

    if pending_state is not None:
        unpaired_states += 1

    samples.sort(key=lambda item: (item.time_s, item.packet_id))

    print(f"解析完成：{len(samples)} 个队列状态样本")
    if samples:
        print(f"检测到端口：{sorted({sample.port for sample in samples})}")
        counts = Counter(sample.action for sample in samples)
        print("缓存决策计数：" + ", ".join(f"{k}={v}" for k, v in counts.items()))

    if debug:
        print(f"未配对的队列状态行：{unpaired_states}", file=sys.stderr)
        print(f"未配对的动作行：{unmatched_action_lines}", file=sys.stderr)

    return samples


def aggregate_values(values: Iterable[float], mode: str) -> float:
    values = list(values)
    if not values:
        return 0.0
    if mode == "sum":
        return sum(values)
    if mode == "max":
        return max(values)
    if mode == "mean":
        return sum(values) / len(values)
    raise ValueError(f"未知聚合方式：{mode}")


def compress_same_timestamp(points: Sequence[Point]) -> List[Point]:
    """
    相同时间戳保留最后一个状态。
    """
    latest: Dict[float, float] = {}
    for time_s, value in points:
        latest[time_s] = value
    return sorted(latest.items())


def decimate(points: Sequence[Point], max_points: int) -> List[Point]:
    if max_points <= 0 or len(points) <= max_points:
        return list(points)

    step = max(1, math.ceil(len(points) / max_points))
    selected = list(points[::step])
    if selected[-1] != points[-1]:
        selected.append(points[-1])
    return selected


def build_port_queue_series(
    samples: Sequence[QueueSample],
    aggregate: str,
    selected_port: Optional[int],
    per_port: bool,
) -> Dict[str, List[Point]]:
    """
    端口队列长度是端口级状态。

    - --per-port：每个端口分别绘制。
    - --port N：只绘制端口 N。
    - 默认：维护每个端口的最新状态，再按 sum/max/mean 聚合。
    """
    if per_port:
        result: DefaultDict[str, List[Point]] = defaultdict(list)
        for sample in samples:
            if selected_port is not None and sample.port != selected_port:
                continue
            result[f"Port {sample.port} queue"].append(
                (sample.time_s, sample.port_queue_bytes / 1024.0)
            )
        return dict(result)

    if selected_port is not None:
        return {
            f"Port {selected_port} queue": [
                (sample.time_s, sample.port_queue_bytes / 1024.0)
                for sample in samples
                if sample.port == selected_port
            ]
        }

    states: Dict[int, float] = {}
    points: List[Point] = []

    for sample in samples:
        states[sample.port] = sample.port_queue_bytes
        aggregate_bytes = aggregate_values(states.values(), aggregate)
        points.append((sample.time_s, aggregate_bytes / 1024.0))

    return {f"Port queues ({aggregate})": points}


def build_buffer_usage_series(
    samples: Sequence[QueueSample],
    sram_capacity_bytes: Optional[float],
    dram_capacity_bytes: Optional[float],
) -> Tuple[Dict[str, List[Point]], float, float]:
    """
    SRAM/DRAM 是全局剩余容量，不能按端口求和。

    未显式指定容量时，使用日志中出现过的最大“剩余容量”作为总容量。
    该方法要求日志至少包含一次接近空缓存的状态。
    """
    if not samples:
        return {}, 0.0, 0.0

    inferred_sram_capacity = max(sample.sram_remaining_bytes for sample in samples)
    inferred_dram_capacity = max(sample.dram_remaining_bytes for sample in samples)

    sram_capacity = (
        sram_capacity_bytes
        if sram_capacity_bytes is not None
        else inferred_sram_capacity
    )
    dram_capacity = (
        dram_capacity_bytes
        if dram_capacity_bytes is not None
        else inferred_dram_capacity
    )

    sram_points: List[Point] = []
    dram_points: List[Point] = []

    for sample in samples:
        sram_used = max(0.0, sram_capacity - sample.sram_remaining_bytes)
        dram_used = max(0.0, dram_capacity - sample.dram_remaining_bytes)

        sram_points.append((sample.time_s, sram_used / 1024.0))
        dram_points.append((sample.time_s, dram_used / 1024.0))

    return (
        {
            "SRAM used": sram_points,
            "DRAM used": dram_points,
        },
        sram_capacity,
        dram_capacity,
    )


def plot_series(
    series: Dict[str, List[Point]],
    output: Path,
    show: bool,
    start_ms: Optional[float],
    end_ms: Optional[float],
    max_points: int,
    title: str,
) -> None:
    import matplotlib.pyplot as plt

    cleaned = {
        label: compress_same_timestamp(points)
        for label, points in series.items()
        if points
    }
    if not cleaned:
        raise RuntimeError("没有可绘制的队列/缓存数据。")

    origin_s = min(points[0][0] for points in cleaned.values())

    fig, ax = plt.subplots(figsize=(12, 6))
    plotted = 0

    for label, raw_points in cleaned.items():
        points_ms: List[Point] = []

        for time_s, value_kib in raw_points:
            time_ms = (time_s - origin_s) * 1000.0

            if start_ms is not None and time_ms < start_ms:
                continue
            if end_ms is not None and time_ms > end_ms:
                continue

            points_ms.append((time_ms, value_kib))

        points_ms = decimate(points_ms, max_points)
        if not points_ms:
            continue

        ax.plot(
            [point[0] for point in points_ms],
            [point[1] for point in points_ms],
            linewidth=1.3,
            drawstyle="steps-post",
            label=label,
        )
        plotted += 1

    if plotted == 0:
        raise RuntimeError("指定时间范围内没有数据。")

    ax.set_xlabel("Time since first queue sample (ms)")
    ax.set_ylabel("Occupancy / used buffer (KiB)")
    ax.set_title(title)
    ax.grid(True, linestyle="--", alpha=0.45)
    ax.legend(fontsize=9, loc="best")

    fig.tight_layout()
    fig.savefig(output, dpi=300, bbox_inches="tight")
    print(f"已保存队列图：{output}")

    if show:
        plt.show()
    else:
        plt.close(fig)


def print_statistics(
    samples: Sequence[QueueSample],
    sram_capacity: float,
    dram_capacity: float,
) -> None:
    if not samples:
        return

    max_queue = max(sample.port_queue_bytes for sample in samples)
    max_sram_used = max(
        max(0.0, sram_capacity - sample.sram_remaining_bytes)
        for sample in samples
    )
    max_dram_used = max(
        max(0.0, dram_capacity - sample.dram_remaining_bytes)
        for sample in samples
    )

    print(f"推断/指定 SRAM 总容量：{sram_capacity / 1024.0:.3f} KiB")
    print(f"推断/指定 DRAM 总容量：{dram_capacity / 1024.0:.3f} KiB")
    print(f"最大单端口队列长度：{max_queue / 1024.0:.3f} KiB")
    print(f"最大 SRAM 使用量：{max_sram_used / 1024.0:.3f} KiB")
    print(f"最大 DRAM 使用量：{max_dram_used / 1024.0:.3f} KiB")


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="绘制当前 FBM 日志中的端口队列、SRAM 使用量和 DRAM 使用量。"
    )
    parser.add_argument(
        "input",
        nargs="?",
        default="filtered_output.txt",
        type=Path,
        help="完整实验日志或同时保留队列状态行和 Time 行的过滤日志。",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("queue_dynamics_fixed.png"),
        help="输出图片路径。",
    )
    parser.add_argument(
        "--port",
        type=int,
        help="只绘制指定端口的队列长度，例如 --port 0。",
    )
    parser.add_argument(
        "--per-port",
        action="store_true",
        help="每个端口分别绘制队列长度。",
    )
    parser.add_argument(
        "--aggregate",
        choices=("sum", "max", "mean"),
        default="sum",
        help="未指定端口且未使用 --per-port 时的端口聚合方式。",
    )
    parser.add_argument(
        "--sram-capacity-bytes",
        type=float,
        help="显式指定 SRAM 总容量；未指定时取日志中最大剩余量。",
    )
    parser.add_argument(
        "--dram-capacity-bytes",
        type=float,
        help="显式指定 DRAM 总容量；未指定时取日志中最大剩余量。",
    )
    parser.add_argument("--start-ms", type=float, help="绘制起始相对时间。")
    parser.add_argument("--end-ms", type=float, help="绘制结束相对时间。")
    parser.add_argument(
        "--max-points",
        type=int,
        default=100000,
        help="每条曲线最多绘制的数据点数。",
    )
    parser.add_argument(
        "--title",
        default="Port Queue and Hybrid Buffer Dynamics",
        help="图标题。",
    )
    parser.add_argument(
        "--no-show",
        action="store_true",
        help="只保存图片，不弹出窗口。",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="打印未配对日志信息。",
    )
    return parser


def main() -> int:
    args = build_argument_parser().parse_args()

    if not args.input.is_file():
        print(f"错误：日志文件不存在：{args.input}", file=sys.stderr)
        return 2

    samples = parse_queue_log(args.input, debug=args.debug)
    if not samples:
        print(
            "错误：未解析到数据。当前脚本要求日志同时包含：\n"
            "  1. 当前端口队列长度/剩余SRAM缓存/剩余DRAM缓存\n"
            "  2. 紧随其后的 Time/packet/端口/存入片内或片外\n"
            "若 filtered_output.txt 只有 TCP State，请改用完整实验日志。",
            file=sys.stderr,
        )
        return 1

    port_series = build_port_queue_series(
        samples,
        aggregate=args.aggregate,
        selected_port=args.port,
        per_port=args.per_port,
    )

    buffer_series, sram_capacity, dram_capacity = build_buffer_usage_series(
        samples,
        sram_capacity_bytes=args.sram_capacity_bytes,
        dram_capacity_bytes=args.dram_capacity_bytes,
    )

    all_series = {}
    all_series.update(port_series)
    all_series.update(buffer_series)

    print_statistics(samples, sram_capacity, dram_capacity)

    try:
        plot_series(
            all_series,
            output=args.output,
            show=not args.no_show,
            start_ms=args.start_ms,
            end_ms=args.end_ms,
            max_points=args.max_points,
            title=args.title,
        )
    except RuntimeError as exc:
        print(f"错误：{exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
