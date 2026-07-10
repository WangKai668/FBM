#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import math
import re
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

import matplotlib.pyplot as plt


UINT32_MAX = 2**32 - 1

TIME_RE = re.compile(
    r"TCP State\s*-\s*"
    r"(?P<value>[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)"
)
CWND_RE = re.compile(r"\bCWND:\s*(?P<value>\d+)\s*bytes")
SSTHRESH_RE = re.compile(r"\bSSTHRESH:\s*(?P<value>\d+)\s*bytes")
INFLIGHT_RE = re.compile(r"\bInFlight:\s*(?P<value>\d+)\s*bytes")
SEGMENT_SIZE_RE = re.compile(r"\bsegment-size:\s*(?P<value>\d+)")
ECN_STATE_RE = re.compile(r"\bECN-State:\s*(?P<value>\S+)")
CONG_STATE_RE = re.compile(r"\bCongState:\s*(?P<value>\S+)")


@dataclass
class TcpSample:
    time_s: float
    cwnd_bytes: int
    ssthresh_bytes: int
    inflight_bytes: int
    segment_size: int
    ecn_state: str
    congestion_state: str


def _match_int(pattern: re.Pattern, line: str) -> Optional[int]:
    match = pattern.search(line)
    return int(match.group("value")) if match else None


def _match_text(pattern: re.Pattern, line: str, default: str = "UNKNOWN") -> str:
    match = pattern.search(line)
    return match.group("value") if match else default


def parse_tcp_data(filename: str, default_segment_size: int = 1426) -> List[TcpSample]:
    """
    解析 TCP 状态日志。

    关键修正：
    1. 使用日志中的 segment-size，而不是固定除以 1500。
    2. 支持普通小数和科学计数法时间。
    3. 保留原始字节值，在绘图阶段再换算单位。
    """
    samples: List[TcpSample] = []
    unmatched_tcp_lines = 0

    with open(filename, "r", encoding="utf-8", errors="replace") as file:
        for line_number, line in enumerate(file, start=1):
            if "TCP State" not in line:
                continue

            time_match = TIME_RE.search(line)
            cwnd = _match_int(CWND_RE, line)
            ssthresh = _match_int(SSTHRESH_RE, line)
            inflight = _match_int(INFLIGHT_RE, line)
            segment_size = _match_int(SEGMENT_SIZE_RE, line)

            if not time_match or cwnd is None or ssthresh is None or inflight is None:
                unmatched_tcp_lines += 1
                if unmatched_tcp_lines <= 5:
                    print(f"警告：第 {line_number} 行 TCP 数据字段不完整，已跳过：")
                    print(line.strip())
                continue

            if segment_size is None or segment_size <= 0:
                segment_size = default_segment_size

            samples.append(
                TcpSample(
                    time_s=float(time_match.group("value")),
                    cwnd_bytes=cwnd,
                    ssthresh_bytes=ssthresh,
                    inflight_bytes=inflight,
                    segment_size=segment_size,
                    ecn_state=_match_text(ECN_STATE_RE, line),
                    congestion_state=_match_text(CONG_STATE_RE, line),
                )
            )

    samples.sort(key=lambda sample: sample.time_s)

    print(f"解析完成：{len(samples)} 个 TCP 状态点")
    if unmatched_tcp_lines:
        print(f"跳过字段不完整的 TCP 行：{unmatched_tcp_lines} 行")

    segment_sizes = sorted({sample.segment_size for sample in samples})
    if segment_sizes:
        print(f"日志中的 TCP segment-size：{segment_sizes}")

    if samples:
        ecn_states = sorted({sample.ecn_state for sample in samples})
        print(f"检测到的 ECN 状态：{ecn_states}")

    print(
        "注意：日志中没有 FlowId/SocketId。若实验包含多条 TCP 流，"
        "这些记录仍会混合在同一组曲线中。"
    )

    return samples


def convert_value(value_bytes: int, segment_size: int, unit: str) -> float:
    if unit == "segments":
        return value_bytes / segment_size
    if unit == "kb":
        return value_bytes / 1024.0
    if unit == "bytes":
        return float(value_bytes)
    raise ValueError(f"不支持的单位：{unit}")


def plot_tcp_data(
    samples: List[TcpSample],
    output_file: str = "tcp_metrics_fixed.png",
    unit: str = "segments",
    show: bool = True,
) -> None:
    if not samples:
        raise ValueError("没有可绘制的 TCP 数据")

    start_time = samples[0].time_s
    times_ms = [(sample.time_s - start_time) * 1000.0 for sample in samples]

    cwnds = [
        convert_value(sample.cwnd_bytes, sample.segment_size, unit)
        for sample in samples
    ]
    inflights = [
        convert_value(sample.inflight_bytes, sample.segment_size, unit)
        for sample in samples
    ]

    # UINT32_MAX 通常表示“尚未设置/近似无限”的初始 SSTHRESH。
    # 将其设为 NaN，避免图中出现从顶部落下的伪竖线。
    ssthreshs = []
    for sample in samples:
        if sample.ssthresh_bytes >= UINT32_MAX:
            ssthreshs.append(math.nan)
        else:
            ssthreshs.append(
                convert_value(
                    sample.ssthresh_bytes,
                    sample.segment_size,
                    unit,
                )
            )

    fig, ax = plt.subplots(figsize=(12, 6))

    ax.plot(
        times_ms,
        cwnds,
        label="CWND",
        linewidth=2.0,
    )
    ax.plot(
        times_ms,
        inflights,
        label="InFlight",
        linewidth=1.3,
        alpha=0.85,
    )

    if any(math.isfinite(value) for value in ssthreshs):
        ax.plot(
            times_ms,
            ssthreshs,
            label="SSTHRESH",
            linestyle="--",
            linewidth=1.6,
            drawstyle="steps-post",
        )

    # 标记第一次出现 ECN 反馈的位置。
    first_ecn_index = next(
        (
            index
            for index, sample in enumerate(samples)
            if sample.ecn_state not in {"ECN_IDLE", "UNKNOWN"}
        ),
        None,
    )
    if first_ecn_index is not None:
        first_ecn_time = times_ms[first_ecn_index]
        ax.axvline(
            first_ecn_time,
            linestyle=":",
            linewidth=1.2,
            label=f"First ECN feedback ({first_ecn_time:.3f} ms)",
        )

    if unit == "segments":
        ylabel = "TCP segments (using segment-size from log)"
    elif unit == "kb":
        ylabel = "Window / InFlight (KiB)"
    else:
        ylabel = "Window / InFlight (bytes)"

    ax.set_xlabel("Time since first TCP sample (ms)")
    ax.set_ylabel(ylabel)
    ax.set_title("TCP Congestion Window Dynamics")
    ax.grid(True, linestyle="--", alpha=0.5)
    ax.legend()

    # 只使用 CWND 和 InFlight 确定纵轴，避免异常 SSTHRESH 放大坐标轴。
    finite_main_values = [
        value
        for value in (cwnds + inflights)
        if math.isfinite(value)
    ]
    if finite_main_values:
        y_max = max(finite_main_values)
        ax.set_ylim(0, y_max * 1.08 if y_max > 0 else 1)

    fig.tight_layout()
    fig.savefig(output_file, dpi=300, bbox_inches="tight")
    print(f"图片已保存：{output_file}")

    if show:
        plt.show()
    else:
        plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="解析并绘制 ns-3 TCP CWND、SSTHRESH 和 InFlight 数据"
    )
    parser.add_argument(
        "input_file",
        nargs="?",
        default="filtered_output.txt",
        help="输入日志文件，默认：filtered_output.txt",
    )
    parser.add_argument(
        "-o",
        "--output",
        default="tcp_metrics_fixed.png",
        help="输出图片路径，默认：tcp_metrics_fixed.png",
    )
    parser.add_argument(
        "--unit",
        choices=("segments", "kb", "bytes"),
        default="segments",
        help="纵轴单位，默认：segments",
    )
    parser.add_argument(
        "--default-segment-size",
        type=int,
        default=1426,
        help="日志缺少 segment-size 时使用的默认值，默认：1426",
    )
    parser.add_argument(
        "--no-show",
        action="store_true",
        help="只保存图片，不弹出绘图窗口",
    )
    args = parser.parse_args()

    input_path = Path(args.input_file)
    if not input_path.is_file():
        raise SystemExit(f"错误：文件不存在：{input_path}")

    samples = parse_tcp_data(
        str(input_path),
        default_segment_size=args.default_segment_size,
    )
    if not samples:
        raise SystemExit("错误：未解析到有效 TCP 数据")

    plot_tcp_data(
        samples,
        output_file=args.output,
        unit=args.unit,
        show=not args.no_show,
    )


if __name__ == "__main__":
    main()
