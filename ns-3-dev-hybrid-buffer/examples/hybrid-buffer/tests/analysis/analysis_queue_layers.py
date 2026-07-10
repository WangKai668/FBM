#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
analysis_queue_layers.py
直接读取 ns-3 原始日志中的：
- MMU_PQS
- RED_QDISC_QUEUE
- P2P_REORDER_QUEUE

按指定 port / priority / queue 输出三个 CSV，并生成三张独立图片。
三层队列量级不同，因此不强行画在同一个纵轴中。

使用：
python3 analysis_queue_layers.py   /home/sj/FBM1/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/pbs/tc2-05/hybrid-buffer-test-tc2-05-sj-tcp.txt   --port 0   --priority 0   --queue 0   --bin-us 1   --out-dir figures_queue_port0
"""

from __future__ import annotations

import argparse
import csv
import math
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


def to_int(value: str | None, default: int = -1) -> int:
    try:
        return int(value) if value is not None else default
    except ValueError:
        return default


def parse_queues(log_path: Path) -> dict[str, list[dict]]:
    data = {"mmu": [], "red": [], "p2p": []}

    with log_path.open("r", encoding="utf-8", errors="ignore") as file:
        for line in file:
            record = parse_kv_line(line, "MMU_PQS")
            if record is not None:
                try:
                    data["mmu"].append(
                        {
                            "time_s": float(record["time_s"]),
                            "port": int(record["port"]),
                            "priority": int(record["priority"]),
                            "queue": int(record["queue"]),
                            "bytes": int(record["bytes"]),
                        }
                    )
                except (KeyError, ValueError):
                    pass
                continue

            record = parse_kv_line(line, "RED_QDISC_QUEUE")
            if record is not None:
                try:
                    data["red"].append(
                        {
                            "time_s": float(record["time_s"]),
                            "port": int(record["port"]),
                            "priority": int(record["priority"]),
                            "queue": int(record["queue"]),
                            "old_bytes": to_int(record.get("old_bytes"), 0),
                            "bytes": int(record["bytes"]),
                        }
                    )
                except (KeyError, ValueError):
                    pass
                continue

            record = parse_kv_line(line, "P2P_REORDER_QUEUE")
            if record is not None:
                try:
                    data["p2p"].append(
                        {
                            "time_s": float(record["time_s"]),
                            "port": int(record["port"]),
                            "bytes": int(record["bytes"]),
                            "packets": to_int(record.get("packets"), 0),
                        }
                    )
                except (KeyError, ValueError):
                    pass

    return data


def in_time_range(
    time_s: float,
    start_ms: float | None,
    end_ms: float | None,
) -> bool:
    time_ms = time_s * 1000.0
    if start_ms is not None and time_ms < start_ms:
        return False
    if end_ms is not None and time_ms > end_ms:
        return False
    return True


def thin_rows(rows: list[dict], max_points: int) -> list[dict]:
    if len(rows) <= max_points:
        return rows
    step = math.ceil(len(rows) / max_points)
    result = rows[::step]
    if result[-1] != rows[-1]:
        result.append(rows[-1])
    return result


def write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        return
    fields = list(rows[0].keys())
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def plot_bytes_queue(
    rows: list[dict],
    output: Path,
    title: str,
    max_points: int,
    unit: str,
) -> None:
    if not rows:
        print(f"Skip {output.name}: no matching data.")
        return

    divisor = 1024.0 * 1024.0 if unit == "MiB" else 1024.0

    plt.figure(figsize=(11, 5.5))
    plt.step(
        [row["time_s"] * 1000.0 for row in rows],
        [row["bytes"] / divisor for row in rows],
        where="post",
    )
    plt.xlabel("Time (ms)")
    plt.ylabel(f"Queue occupancy ({unit})")
    plt.title(title)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(output, dpi=200)
    plt.close()


def plot_p2p_queue(
    rows: list[dict],
    output: Path,
    title: str,
    max_points: int,
) -> None:
    if not rows:
        print(f"Skip {output.name}: no matching data.")
        return

    plt.figure(figsize=(11, 5.5))
    plt.step(
        [row["time_s"] * 1000.0 for row in rows],
        [row["packets"] for row in rows],
        where="post",
        label="Packets",
    )
    plt.xlabel("Time (ms)")
    plt.ylabel("Queue occupancy (packets)")
    plt.title(title)
    plt.yticks(sorted({row["packets"] for row in rows}))
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(output, dpi=200)
    plt.close()

def aggregate_max_by_time(rows, bin_us, value_key):
    if not rows:
        return []

    bins = {}

    for row in rows:
        bin_id = int(row["time_s"] * 1e6 / bin_us)

        if bin_id not in bins:
            bins[bin_id] = row.copy()
        elif row[value_key] > bins[bin_id][value_key]:
            bins[bin_id] = row.copy()

    return sorted(bins.values(), key=lambda row: row["time_s"])

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path, help="ns-3 raw log file")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--priority", type=int, default=0)
    parser.add_argument("--queue", type=int, default=0)
    parser.add_argument("--start-ms", type=float)
    parser.add_argument("--end-ms", type=float)
    parser.add_argument("--max-points", type=int, default=12000)

    parser.add_argument(
        "--bin-us",
        type=float,
        default=1.0,
        help="聚合时间窗口，单位微秒；设置为0表示不聚合",
    )

    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("figures_queues"),
    )
    args = parser.parse_args()

    if not args.log.is_file():
        raise FileNotFoundError(args.log)

    data = parse_queues(args.log)

    mmu = [
        row
        for row in data["mmu"]
        if row["port"] == args.port
        and row["priority"] == args.priority
        and row["queue"] == args.queue
        and in_time_range(
            row["time_s"],
            args.start_ms,
            args.end_ms,
        )
    ]

    red = [
        row
        for row in data["red"]
        if row["port"] == args.port
        and row["priority"] == args.priority
        and row["queue"] == args.queue
        and in_time_range(
            row["time_s"],
            args.start_ms,
            args.end_ms,
        )
    ]

    p2p = [
        row
        for row in data["p2p"]
        if row["port"] == args.port
        and in_time_range(
            row["time_s"],
            args.start_ms,
            args.end_ms,
        )
    ]

    # 每个时间窗口取最大队列长度
    mmu_plot = aggregate_max_by_time(
        mmu,
        args.bin_us,
        "bytes",
    )

    red_plot = aggregate_max_by_time(
        red,
        args.bin_us,
        "bytes",
    )

    p2p_plot = aggregate_max_by_time(
        p2p,
        args.bin_us,
        "packets",
    )

    args.out_dir.mkdir(
        parents=True,
        exist_ok=True,
    )

    # 保存用于绘图的聚合数据
    write_csv(
        args.out_dir / "mmu_pqs.csv",
        mmu_plot,
    )

    write_csv(
        args.out_dir / "red_qdisc_queue.csv",
        red_plot,
    )

    write_csv(
        args.out_dir / "p2p_reorder_queue.csv",
        p2p_plot,
    )

    # 注意：这里必须使用 mmu_plot，而不是 mmu
    plot_bytes_queue(
        mmu_plot,
        args.out_dir / "mmu_pqs_queue.png",
        (
            f"MMU_PQS queue: port={args.port}, "
            f"priority={args.priority}, "
            f"queue={args.queue}"
        ),
        args.max_points,
        "MiB",
    )

    # 注意：这里必须使用 red_plot，而不是 red
    plot_bytes_queue(
        red_plot,
        args.out_dir / "red_qdisc_queue.png",
        (
            f"RedQueueDisc queue: port={args.port}, "
            f"priority={args.priority}, "
            f"queue={args.queue}"
        ),
        args.max_points,
        "MiB",
    )

    # 注意：这里必须使用 p2p_plot，而不是 p2p
    plot_p2p_queue(
        p2p_plot,
        args.out_dir / "p2p_reorder_queue.png",
        (
            "PointToPointReorderNetDevice queue: "
            f"port={args.port}"
        ),
        args.max_points,
    )

    print("========== Raw records ==========")
    print(f"MMU_PQS raw records: {len(mmu)}")
    print(f"RED_QDISC_QUEUE raw records: {len(red)}")
    print(f"P2P_REORDER_QUEUE raw records: {len(p2p)}")

    print("========== Aggregated records ===")
    print(f"MMU_PQS plot records: {len(mmu_plot)}")
    print(f"RED plot records: {len(red_plot)}")
    print(f"P2P plot records: {len(p2p_plot)}")

    if mmu:
        print(
            f"MMU peak: "
            f"{max(row['bytes'] for row in mmu)} bytes"
        )

    if red:
        print(
            f"RED peak: "
            f"{max(row['bytes'] for row in red)} bytes"
        )

    if p2p:
        print(
            f"P2P peak: "
            f"{max(row['packets'] for row in p2p)} packets"
        )

    print(f"Output directory: {args.out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())