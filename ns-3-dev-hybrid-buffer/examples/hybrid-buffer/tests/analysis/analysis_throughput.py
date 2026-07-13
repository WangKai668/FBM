#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
画出所有交换机端口(port)和所有主机(host)的吞吐量到同一张图。

输入文件格式：
- port-throughput-test-tc2-05-p0.csv
- host-throughput-test-tc2-05-n2.csv

使用示例：
python3 analysis_throughput.py   /home/sj/FBM1/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/pbs/tc2-05   --sim-name test-tc2-05   --start-ms 0   --end-ms 26   -o all_ports_throughput.png
python3 analysis_throughput.py   /home/wk/FBM/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/BMS/tc2-05   --sim-name test-tc2-05   --start-ms 0   --end-ms 26   -o all_ports_throughput.png
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


PORT_RE = re.compile(r"-p(\d+)\.csv$")
HOST_RE = re.compile(r"-n(\d+)\.csv$")


def read_throughput_csv(path: Path) -> tuple[list[float], list[float]]:
    times_ms: list[float] = []
    rates_gbps: list[float] = []

    with path.open("r", newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)

        if not reader.fieldnames:
            raise ValueError(f"empty CSV: {path}")

        rate_column = next(
            (
                name
                for name in ("sendRate", "receiveRate", "recvRate", "rate", "throughput")
                if name in reader.fieldnames
            ),
            None,
        )

        if rate_column is None or "start" not in reader.fieldnames or "end" not in reader.fieldnames:
            raise ValueError(
                f"{path} must contain start,end and one of "
                f"sendRate/receiveRate/recvRate/rate/throughput"
            )

        for row_number, row in enumerate(reader, start=2):
            try:
                start_s = float(row["start"])
                end_s = float(row["end"])
                rate_bps = float(row[rate_column])
            except (TypeError, ValueError) as exc:
                raise ValueError(f"invalid numeric value at {path}:{row_number}") from exc

            times_ms.append((start_s + end_s) * 500.0)
            rates_gbps.append(rate_bps / 1e9)

    if not times_ms:
        raise ValueError(f"no throughput samples in {path}")

    return times_ms, rates_gbps


def moving_average(values: list[float], window: int) -> list[float]:
    if window <= 1 or len(values) <= 1:
        return values

    window = min(window, len(values))
    result: list[float] = []
    running_sum = 0.0
    history: list[float] = []

    for value in values:
        history.append(value)
        running_sum += value

        if len(history) > window:
            running_sum -= history[-window - 1]

        result.append(running_sum / min(len(history), window))

    return result


def extract_port(path: Path) -> int:
    match = PORT_RE.search(path.name)
    if not match:
        raise ValueError(f"cannot extract port number from {path.name}")
    return int(match.group(1))


def extract_host(path: Path) -> int:
    match = HOST_RE.search(path.name)
    if not match:
        raise ValueError(f"cannot extract host number from {path.name}")
    return int(match.group(1))


def discover_files(data_dir: Path, sim_name: str) -> tuple[list[tuple[int, Path]], list[tuple[int, Path]]]:
    port_files = sorted(data_dir.glob(f"port-throughput-{sim_name}-p*.csv"))
    host_files = sorted(data_dir.glob(f"host-throughput-{sim_name}-n*.csv"))

    if not port_files:
        raise FileNotFoundError(f"no port files found in {data_dir} for sim-name={sim_name}")
    if not host_files:
        raise FileNotFoundError(f"no host files found in {data_dir} for sim-name={sim_name}")

    ports = sorted((extract_port(path), path) for path in port_files)
    hosts = sorted((extract_host(path), path) for path in host_files)

    return ports, hosts


def parse_filter(text: str | None) -> set[int] | None:
    if not text:
        return None

    result: set[int] = set()

    for item in text.split(","):
        item = item.strip()
        if not item:
            continue

        if "-" in item:
            left, right = item.split("-", 1)
            start = int(left)
            end = int(right)
            if start > end:
                start, end = end, start
            result.update(range(start, end + 1))
        else:
            result.add(int(item))

    return result


def plot_all(
    ports: list[tuple[int, Path]],
    hosts: list[tuple[int, Path]],
    output: Path,
    smooth_window: int,
    start_ms: float | None,
    end_ms: float | None,
    title: str,
    legend_columns: int,
    show: bool,
) -> None:
    fig, ax = plt.subplots(figsize=(14, 8))

    # 交换机端口：实线
    for port, path in ports:
        t, y = read_throughput_csv(path)
        y = moving_average(y, smooth_window)
        ax.plot(t, y, linewidth=1.2, linestyle="-", label=f"Port {port}")

    # 主机：虚线
    for host, path in hosts:
        t, y = read_throughput_csv(path)
        y = moving_average(y, smooth_window)
        ax.plot(t, y, linewidth=1.0, linestyle="--", label=f"Host {host}")

    if start_ms is not None or end_ms is not None:
        left, right = ax.get_xlim()
        ax.set_xlim(start_ms if start_ms is not None else left,
                    end_ms if end_ms is not None else right)

    ax.set_xlabel("Time (ms)")
    ax.set_ylabel("Throughput (Gbps)")
    ax.set_title(title)
    ax.grid(True, linestyle="--", alpha=0.35)

    ax.legend(
        loc="upper center",
        bbox_to_anchor=(0.5, -0.12),
        ncol=max(1, legend_columns),
        fontsize=8,
    )

    fig.tight_layout()
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=300, bbox_inches="tight")

    if show:
        plt.show()

    plt.close(fig)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Plot all switch ports and all hosts throughput in one figure."
    )
    parser.add_argument("data_dir", type=Path, help="directory containing throughput csv files")
    parser.add_argument("--sim-name", default="test-tc2-05")
    parser.add_argument("--ports", default=None, help="optional port filter, e.g. 0-5 or 0,1,3")
    parser.add_argument("--hosts", default=None, help="optional host filter, e.g. 0-5 or 0,2,4")
    parser.add_argument("--smooth-window", type=int, default=1)
    parser.add_argument("--start-ms", type=float, default=None)
    parser.add_argument("--end-ms", type=float, default=None)
    parser.add_argument("--legend-columns", type=int, default=6)
    parser.add_argument("--title", default="All switch ports and hosts throughput")
    parser.add_argument("-o", "--output", type=Path, default=Path("all_ports_hosts_throughput.png"))
    parser.add_argument("--show", action="store_true")
    return parser


def main() -> int:
    args = build_parser().parse_args()

    try:
        port_files, host_files = discover_files(args.data_dir, args.sim_name)

        selected_ports = parse_filter(args.ports)
        selected_hosts = parse_filter(args.hosts)

        if selected_ports is not None:
            port_files = [(port, path) for port, path in port_files if port in selected_ports]

        if selected_hosts is not None:
            host_files = [(host, path) for host, path in host_files if host in selected_hosts]

        if not port_files:
            raise ValueError("no port files remain after applying --ports")
        if not host_files:
            raise ValueError("no host files remain after applying --hosts")

        plot_all(
            ports=port_files,
            hosts=host_files,
            output=args.output,
            smooth_window=max(1, args.smooth_window),
            start_ms=args.start_ms,
            end_ms=args.end_ms,
            title=args.title,
            legend_columns=max(1, args.legend_columns),
            show=args.show,
        )

    except (OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    print("Ports plotted:")
    for port, path in port_files:
        print(f"  Port {port}: {path}")

    print("Hosts plotted:")
    for host, path in host_files:
        print(f"  Host {host}: {path}")

    print(f"Saved: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())