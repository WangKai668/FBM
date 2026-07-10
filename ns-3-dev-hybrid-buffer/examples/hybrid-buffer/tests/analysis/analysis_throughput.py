#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Plot host transmit throughput and switch-port throughput for the FBM/ns-3 project.

Supported input sources
-----------------------
1. Native FBM switch-port CSV files:
   port-throughput-<simName>-p<port>.csv
   Header: start,end,sendRate
   sendRate is interpreted as bit/s.

2. Optional host-throughput CSV files:
   host-throughput-*.csv
   Common columns such as start,end,sendRate or time,throughput are supported.

3. A text log such as filtered_output.txt. The parser accepts lines containing:
   - "Switch Port <id> ... Throughput: <value> Gbps"
   - "Host Node <id> ... Throughput: <value> Gbps"
   The field order may vary, provided that the line also contains a time value.

Examples
--------
python3 analysis_throughput.py filtered_output.txt
python3 analysis_throughput.py filtered_output.txt --data-dir ../data/BMS/tc2-05
python3 analysis_throughput.py --data-dir ../data/pbs/tc2-05 --no-show
"""

from __future__ import annotations

import argparse
import csv
import math
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import DefaultDict, Dict, Iterable, List, Optional, Sequence, Tuple

Point = Tuple[float, float]  # time in seconds, throughput in Gbps
Series = DefaultDict[str, List[Point]]

NUMBER = r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?"

TIME_PATTERNS = (
    re.compile(
        rf"(?:Time(?:\s+since\s+start)?|Timestamp|time|timestamp|\bt)\s*[:=]\s*"
        rf"(?P<value>{NUMBER})\s*(?P<unit>ns|us|µs|ms|s)?",
        re.IGNORECASE,
    ),
    re.compile(
        rf"(?:Throughput|Rate)\s*-\s*(?P<value>{NUMBER})\s*"
        rf"(?P<unit>ns|us|µs|ms|s)?",
        re.IGNORECASE,
    ),
    re.compile(
        rf"^\s*(?P<value>{NUMBER})\s*(?P<unit>ns|us|µs|ms|s)\b",
        re.IGNORECASE,
    ),
)

OBJECT_PATTERN = re.compile(
    r"(?P<kind>Switch[\s_-]*Port|Host[\s_-]*Node)"
    r"(?:\s*(?:ID|id))?\s*[:=#-]?\s*"
    r"(?P<id>0x[0-9a-fA-F]+|\d+)?",
    re.IGNORECASE,
)

RATE_PATTERNS = (
    re.compile(
        rf"(?:Throughput|sendRate|tx[_\s-]*bw|Rate)\s*[:=]\s*"
        rf"(?P<value>{NUMBER})\s*(?P<unit>Tbps|Gbps|Mbps|Kbps|bps)\b",
        re.IGNORECASE,
    ),
    re.compile(
        rf"(?P<value>{NUMBER})\s*(?P<unit>Tbps|Gbps|Mbps|Kbps|bps)\b",
        re.IGNORECASE,
    ),
)

RAW_RATE_PATTERN = re.compile(
    rf"(?:Throughput|sendRate|tx[_\s-]*bw|Rate)\s*[:=]\s*"
    rf"(?P<value>{NUMBER})(?!\s*(?:Tbps|Gbps|Mbps|Kbps|bps)\b)",
    re.IGNORECASE,
)

PORT_FILE_RE = re.compile(r"-p(?P<port>\d+)\.csv$", re.IGNORECASE)
HOST_FILE_RE = re.compile(
    r"(?:host|node)[-_]?(?P<host>0x[0-9a-fA-F]+|\d+)?", re.IGNORECASE
)


def time_to_seconds(value: float, unit: Optional[str]) -> float:
    unit = (unit or "s").lower()
    factors = {
        "s": 1.0,
        "ms": 1e-3,
        "us": 1e-6,
        "µs": 1e-6,
        "ns": 1e-9,
    }
    return value * factors[unit]


def rate_to_gbps(value: float, unit: str) -> float:
    factors = {
        "tbps": 1e3,
        "gbps": 1.0,
        "mbps": 1e-3,
        "kbps": 1e-6,
        "bps": 1e-9,
    }
    return value * factors[unit.lower()]


def extract_time_seconds(line: str) -> Optional[float]:
    for pattern in TIME_PATTERNS:
        match = pattern.search(line)
        if match:
            return time_to_seconds(float(match.group("value")), match.group("unit"))

    # FBM TCP log style: "... State - 1.23e-04 ..."
    match = re.search(rf"\b(?:State|Stats?|Sample)\s*-\s*(?P<value>{NUMBER})\b", line)
    if match:
        return float(match.group("value"))
    return None


def extract_rate_gbps(line: str) -> Optional[float]:
    for pattern in RATE_PATTERNS:
        match = pattern.search(line)
        if match:
            return rate_to_gbps(float(match.group("value")), match.group("unit"))

    # Unit-less custom logs are interpreted heuristically:
    # large values are bit/s; small values are already Gbit/s.
    match = RAW_RATE_PATTERN.search(line)
    if match:
        value = float(match.group("value"))
        return value / 1e9 if abs(value) >= 1e6 else value
    return None


def canonical_label(kind: str, object_id: Optional[str]) -> str:
    normalized = re.sub(r"[\s_-]+", " ", kind).strip().lower()
    prefix = "Switch Port" if normalized.startswith("switch") else "Host Node"
    return f"{prefix} {object_id or 'unknown'}"


def parse_text_log(path: Path, debug: bool = False) -> Series:
    series: Series = defaultdict(list)
    unmatched: List[str] = []

    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            object_match = OBJECT_PATTERN.search(line)
            if not object_match:
                continue

            time_s = extract_time_seconds(line)
            rate_gbps = extract_rate_gbps(line)
            if time_s is None or rate_gbps is None:
                if debug and len(unmatched) < 20:
                    unmatched.append(line.rstrip())
                continue

            label = canonical_label(
                object_match.group("kind"), object_match.group("id")
            )
            series[label].append((time_s, rate_gbps))

    if debug and unmatched:
        print("\n以下吞吐量日志包含对象关键词，但未能解析时间或速率：", file=sys.stderr)
        for line in unmatched:
            print(f"  {line}", file=sys.stderr)

    return series


def first_present(row: Dict[str, str], names: Sequence[str]) -> Optional[str]:
    lowered = {key.strip().lower(): value for key, value in row.items() if key}
    for name in names:
        value = lowered.get(name.lower())
        if value not in (None, ""):
            return value
    return None


def parse_native_throughput_csv(path: Path, label: str) -> List[Point]:
    points: List[Point] = []
    with path.open("r", encoding="utf-8", errors="replace", newline="") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames:
            return points

        for row in reader:
            try:
                start_raw = first_present(row, ("start", "begin", "time_start"))
                end_raw = first_present(row, ("end", "stop", "time_end"))
                time_raw = first_present(row, ("time", "timestamp", "t"))
                rate_raw = first_present(
                    row,
                    (
                        "sendRate",
                        "throughput",
                        "throughput_bps",
                        "throughput_gbps",
                        "rate",
                        "rate_gbps",
                        "sendrate_gbps",
                        "tx_bw",
                    ),
                )

                if rate_raw is None:
                    continue

                if start_raw is not None and end_raw is not None:
                    time_s = (float(start_raw) + float(end_raw)) / 2.0
                elif time_raw is not None:
                    time_s = float(time_raw)
                else:
                    continue

                rate_value = float(rate_raw)

                # Native FBM CSV uses bit/s. Explicit Gbps columns are also supported.
                field_map = {key.strip().lower(): key for key in row if key}
                if any(
                    name in field_map
                    for name in ("throughput_gbps", "rate_gbps", "sendrate_gbps")
                ):
                    rate_gbps = rate_value
                else:
                    rate_gbps = rate_value / 1e9

                if math.isfinite(time_s) and math.isfinite(rate_gbps):
                    points.append((time_s, rate_gbps))
            except (TypeError, ValueError):
                continue
    return points


def load_csv_series(data_dir: Path) -> Series:
    series: Series = defaultdict(list)

    for path in sorted(data_dir.glob("port-throughput-*.csv")):
        match = PORT_FILE_RE.search(path.name)
        port_id = match.group("port") if match else path.stem
        label = f"Switch Port p{port_id}"
        series[label].extend(parse_native_throughput_csv(path, label))

    for pattern in ("host-throughput-*.csv", "node-throughput-*.csv"):
        for path in sorted(data_dir.glob(pattern)):
            match = HOST_FILE_RE.search(path.stem)
            host_id = match.group("host") if match and match.group("host") else path.stem
            label = f"Host Node {host_id}"
            series[label].extend(parse_native_throughput_csv(path, label))

    return series


def merge_series(target: Series, source: Series) -> None:
    for label, points in source.items():
        target[label].extend(points)


def deduplicate_and_sort(points: Iterable[Point]) -> List[Point]:
    # Keep the latest value if the same series has duplicate samples at one timestamp.
    by_time: Dict[float, float] = {}
    for time_s, value in points:
        by_time[time_s] = value
    return sorted(by_time.items())


def clip_points(
    points: Sequence[Point],
    origin_s: float,
    start_ms: Optional[float],
    end_ms: Optional[float],
) -> List[Tuple[float, float]]:
    clipped: List[Tuple[float, float]] = []
    for time_s, value in points:
        time_ms = (time_s - origin_s) * 1000.0
        if start_ms is not None and time_ms < start_ms:
            continue
        if end_ms is not None and time_ms > end_ms:
            continue
        clipped.append((time_ms, value))
    return clipped


def plot_series(
    series: Series,
    output: Path,
    show: bool,
    start_ms: Optional[float],
    end_ms: Optional[float],
    title: str,
) -> None:
    import matplotlib.pyplot as plt

    cleaned = {
        label: deduplicate_and_sort(points)
        for label, points in series.items()
        if points
    }
    if not cleaned:
        raise RuntimeError(
            "未解析到吞吐量数据。请检查输入日志格式，或确认目录中存在 "
            "port-throughput-*.csv。可加 --debug 查看未匹配日志。"
        )

    origin_s = min(points[0][0] for points in cleaned.values())

    fig, ax = plt.subplots(figsize=(14, 7))
    plotted = 0

    def order_key(item: Tuple[str, List[Point]]) -> Tuple[int, str]:
        return (0 if item[0].startswith("Switch Port") else 1, item[0])

    for label, points in sorted(cleaned.items(), key=order_key):
        clipped = clip_points(points, origin_s, start_ms, end_ms)
        if not clipped:
            continue

        times_ms = [point[0] for point in clipped]
        rates_gbps = [point[1] for point in clipped]

        if label.startswith("Switch Port"):
            ax.plot(
                times_ms,
                rates_gbps,
                linewidth=1.6,
                marker="o",
                markersize=2.5,
                markevery=max(1, len(times_ms) // 80),
                label=f"{label} Throughput (Gbps)",
            )
        else:
            ax.plot(
                times_ms,
                rates_gbps,
                linewidth=1.2,
                linestyle="--",
                label=f"{label} Throughput (Gbps)",
            )
        plotted += 1

    if plotted == 0:
        raise RuntimeError("时间范围内没有可绘制的吞吐量数据。")

    ax.set_xlabel("Time since start (ms)")
    ax.set_ylabel("Throughput (Gbps)")
    ax.set_title(title)
    ax.grid(True, linestyle="--", alpha=0.45)
    ax.legend(fontsize=8, loc="best")
    fig.tight_layout()
    fig.savefig(output, dpi=300, bbox_inches="tight")

    print(f"已保存吞吐量图：{output}")
    for label, points in sorted(cleaned.items(), key=order_key):
        print(f"  {label}: {len(points)} points")

    if show:
        plt.show()
    else:
        plt.close(fig)


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="绘制 FBM 主机发送吞吐量和交换机端口吞吐量。"
    )
    parser.add_argument(
        "input",
        nargs="?",
        default="filtered_output.txt",
        help="包含吞吐量打印的日志文件，默认 filtered_output.txt。",
    )
    parser.add_argument(
        "--data-dir",
        type=Path,
        help="实验输出目录；自动读取 port-throughput-*.csv 和可选 host-throughput-*.csv。",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("throughput_dynamics.png"),
        help="输出图片路径。",
    )
    parser.add_argument(
        "--start-ms", type=float, help="只绘制该相对时间之后的数据。"
    )
    parser.add_argument(
        "--end-ms", type=float, help="只绘制该相对时间之前的数据。"
    )
    parser.add_argument(
        "--title",
        default="Switch and Host Throughput Dynamics (Per Port/Node)",
        help="图标题。",
    )
    parser.add_argument(
        "--no-show", action="store_true", help="只保存图片，不弹出窗口。"
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="打印包含对象关键词但未能解析的日志样例。",
    )
    return parser


def main() -> int:
    args = build_argument_parser().parse_args()
    combined: Series = defaultdict(list)

    input_path = Path(args.input)
    if input_path.is_file():
        merge_series(combined, parse_text_log(input_path, debug=args.debug))
    elif args.input != "filtered_output.txt":
        print(f"错误：日志文件不存在：{input_path}", file=sys.stderr)
        return 2

    if args.data_dir:
        if not args.data_dir.is_dir():
            print(f"错误：实验数据目录不存在：{args.data_dir}", file=sys.stderr)
            return 2
        merge_series(combined, load_csv_series(args.data_dir))

    try:
        plot_series(
            combined,
            args.output,
            show=not args.no_show,
            start_ms=args.start_ms,
            end_ms=args.end_ms,
            title=args.title,
        )
    except RuntimeError as exc:
        print(f"错误：{exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
