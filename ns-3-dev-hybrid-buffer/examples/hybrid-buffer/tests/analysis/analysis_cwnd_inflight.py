#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
analysis_cwnd_inflight.py

直接从 ns-3 原始日志中的 TCP_STATE 行读取数据，绘制指定 TCP 连接的：
- CWND
- InFlight
- SSTHRESH

纵轴换算成包数

使用：
python3 analysis_cwnd_inflight.py /home/sj/FBM1/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/pbs/tc2-05/hybrid-buffer-test-tc2-05-sj-tcp.txt  
python3 analysis_cwnd_inflight.py /home/wk/FBM/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/BMS/tc2-05/hybrid-buffer-test-tc2-05.txt
（把名字写清楚就好）
"""

from __future__ import annotations

import argparse
import csv
import math
from collections import Counter
from pathlib import Path

import matplotlib.pyplot as plt

def parse_kv_line(line: str, tag: str) -> dict[str, str] | None:
    if not line.startswith(tag + ","):
        return None
    result: dict[str, str] = {}
    for item in line.strip().split(",")[1:]:
        if "=" not in item:
            continue
        key, value = item.split("=", 1)
        result[key.strip()] = value.strip()
    return result


def parse_tcp_states(log_path: Path) -> list[dict]:
    rows: list[dict] = []

    with log_path.open("r", encoding="utf-8", errors="ignore") as file:
        for line in file:
            record = parse_kv_line(line, "TCP_STATE")
            if record is None:
                continue

            try:
                rows.append(
                    {
                        "time_s": float(record["time_s"]),
                        "conn": record.get("conn", "unknown"),
                        "algorithm": record.get("algorithm", ""),
                        "cwnd_bytes": int(record["cwnd_bytes"]),
                        "inflight_bytes": int(record["inflight_bytes"]),
                        "ssthresh_bytes": int(record["ssthresh_bytes"]),
                        "segment_size": int(record.get("segment_size", "1")),
                        "rtt_us": float(record.get("rtt_us", "nan")),
                        "alpha": float(record.get("alpha", "nan")),
                        "ecn_state": record.get("ecn_state", ""),
                        "cong_state": record.get("cong_state", ""),
                    }
                )
            except (KeyError, ValueError):
                continue

    return rows


def choose_connection(rows: list[dict], requested: str | None) -> str:
    if requested:
        if not any(row["conn"] == requested for row in rows):
            available = ", ".join(sorted({row["conn"] for row in rows}))
            raise ValueError(
                f"Connection {requested} was not found. Available: {available}"
            )
        return requested

    counts = Counter(row["conn"] for row in rows)
    if not counts:
        raise ValueError("No TCP_STATE records were found.")
    return counts.most_common(1)[0][0]


def filter_rows(
    rows: list[dict],
    conn: str,
    start_ms: float | None,
    end_ms: float | None,
) -> list[dict]:
    result = []
    for row in rows:
        if row["conn"] != conn:
            continue
        time_ms = row["time_s"] * 1000.0
        if start_ms is not None and time_ms < start_ms:
            continue
        if end_ms is not None and time_ms > end_ms:
            continue
        result.append(row)
    return sorted(result, key=lambda item: item["time_s"])


def thin_rows(rows: list[dict], max_points: int) -> list[dict]:
    if len(rows) <= max_points:
        return rows

    step = math.ceil(len(rows) / max_points)
    result = rows[::step]
    if result[-1] != rows[-1]:
        result.append(rows[-1])
    return result


def write_csv(path: Path, rows: list[dict]) -> None:
    fields = [
        "time_s",
        "time_ms",
        "conn",
        "algorithm",
        "cwnd_bytes",
        "inflight_bytes",
        "ssthresh_bytes",
        "segment_size",
        "cwnd_packets",
        "inflight_packets",
        "ssthresh_packets",
        "rtt_us",
        "alpha",
        "ecn_state",
        "cong_state",
    ]

    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()

        for row in rows:
            segment_size = max(row["segment_size"], 1)
            ssthresh_packets = ""
            if row["ssthresh_bytes"] < 0xFFFFFFFF:
                ssthresh_packets = row["ssthresh_bytes"] / segment_size

            writer.writerow(
                {
                    **row,
                    "time_ms": row["time_s"] * 1000.0,
                    "cwnd_packets": row["cwnd_bytes"] / segment_size,
                    "inflight_packets": row["inflight_bytes"] / segment_size,
                    "ssthresh_packets": ssthresh_packets,
                }
            )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path, help="ns-3 raw log file")
    parser.add_argument("--conn", help="TCP connection pointer, e.g. 0x557eedcc8380")
    parser.add_argument("--start-ms", type=float)
    parser.add_argument("--end-ms", type=float)
    parser.add_argument("--max-points", type=int, default=10000)
    parser.add_argument("--out-dir", type=Path, default=Path("figures_cwnd"))
    args = parser.parse_args()

    if not args.log.is_file():
        raise FileNotFoundError(args.log)

    all_rows = parse_tcp_states(args.log)
    conn = choose_connection(all_rows, args.conn)
    rows = filter_rows(all_rows, conn, args.start_ms, args.end_ms)

    if not rows:
        raise RuntimeError("No TCP_STATE records remain after filtering.")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    csv_path = args.out_dir / "tcp_cwnd_inflight.csv"
    png_path = args.out_dir / "tcp_cwnd_inflight.png"

    write_csv(csv_path, rows)

    plot_rows = thin_rows(rows, args.max_points)
    time_ms = [row["time_s"] * 1000.0 for row in plot_rows]
    cwnd_packets = [
        row["cwnd_bytes"] / max(row["segment_size"], 1) for row in plot_rows
    ]
    inflight_packets = [
        row["inflight_bytes"] / max(row["segment_size"], 1) for row in plot_rows
    ]
    ssthresh_packets = [
        math.nan
        if row["ssthresh_bytes"] >= 0xFFFFFFFF
        else row["ssthresh_bytes"] / max(row["segment_size"], 1)
        for row in plot_rows
    ]

    plt.figure(figsize=(11, 5.8))
    # plt.plot(time_ms, cwnd_packets, label="CWND")
    # plt.plot(time_ms, inflight_packets, label="InFlight")
    # plt.plot(time_ms, ssthresh_packets, label="SSTHRESH")
    plt.plot(time_ms, cwnd_packets, label="CWND", marker='o', markersize=4, linestyle='-')
    plt.plot(time_ms, inflight_packets, label="InFlight", marker='s', markersize=4, linestyle='-')
    plt.plot(time_ms, ssthresh_packets, label="SSTHRESH", marker='^', markersize=4, linestyle='-')


    ece_rows = [
        row
        for row in plot_rows
        if row["ecn_state"] == "ECN_ECE_RCVD"
    ]
    if ece_rows:
        plt.scatter(
            [row["time_s"] * 1000.0 for row in ece_rows],
            [
                row["cwnd_bytes"] / max(row["segment_size"], 1)
                for row in ece_rows
            ],
            s=12,
            label="ECE received",
        )

    plt.xlabel("Time (ms)")
    plt.ylabel("Packets")
    plt.title(f"TCP congestion window and in-flight packets\nConnection: {conn}")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(png_path, dpi=200)
    plt.close()

    print(f"Selected connection: {conn}")
    print(f"TCP_STATE records: {len(rows)}")
    print(f"CSV: {csv_path}")
    print(f"Figure: {png_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())