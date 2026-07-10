#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
analysis_dctcp_alpha.py

绘制 DCTCP 的 alpha 和 CE 标记比例。
优先读取 DCTCP_ALPHA 行；如果日志中没有 DCTCP_ALPHA，则退化为读取
TCP_STATE 中的 alpha。

使用：
python3 analysis_dctcp_alpha.py /home/sj/FBM1/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/pbs/tc2-05/hybrid-buffer-test-tc2-05-tcp.txt
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


def parse_log(log_path: Path) -> tuple[list[dict], list[dict]]:
    alpha_rows: list[dict] = []
    state_rows: list[dict] = []

    with log_path.open("r", encoding="utf-8", errors="ignore") as file:
        for line in file:
            record = parse_kv_line(line, "DCTCP_ALPHA")
            if record is not None:
                try:
                    alpha_rows.append(
                        {
                            "time_s": float(record["time_s"]),
                            "conn": record.get("conn", "unknown"),
                            "marked_bytes": int(record.get("marked_bytes", "0")),
                            "total_bytes": int(record.get("total_bytes", "0")),
                            "fraction": float(record.get("fraction", "nan")),
                            "g": float(record.get("g", "nan")),
                            "old_alpha": float(record.get("old_alpha", "nan")),
                            "new_alpha": float(record.get("new_alpha", "nan")),
                            "cwnd_bytes": int(record.get("cwnd_bytes", "0")),
                            "ssthresh_bytes": int(
                                record.get("ssthresh_bytes", "0")
                            ),
                        }
                    )
                except (KeyError, ValueError):
                    pass
                continue

            record = parse_kv_line(line, "TCP_STATE")
            if record is not None:
                try:
                    state_rows.append(
                        {
                            "time_s": float(record["time_s"]),
                            "conn": record.get("conn", "unknown"),
                            "alpha": float(record.get("alpha", "nan")),
                            "cwnd_bytes": int(record.get("cwnd_bytes", "0")),
                            "segment_size": int(record.get("segment_size", "1")),
                            "ecn_state": record.get("ecn_state", ""),
                            "cong_state": record.get("cong_state", ""),
                        }
                    )
                except (KeyError, ValueError):
                    pass

    return alpha_rows, state_rows


def choose_connection(
    alpha_rows: list[dict],
    state_rows: list[dict],
    requested: str | None,
) -> str:
    all_connections = {
        row["conn"] for row in alpha_rows
    } | {
        row["conn"] for row in state_rows
    }

    if requested:
        if requested not in all_connections:
            raise ValueError(
                f"Connection {requested} was not found. "
                f"Available: {', '.join(sorted(all_connections))}"
            )
        return requested

    source = alpha_rows if alpha_rows else state_rows
    counts = Counter(row["conn"] for row in source)
    if not counts:
        raise ValueError("No DCTCP_ALPHA or TCP_STATE records were found.")
    return counts.most_common(1)[0][0]


def in_range(
    row: dict,
    conn: str,
    start_ms: float | None,
    end_ms: float | None,
) -> bool:
    if row["conn"] != conn:
        return False
    time_ms = row["time_s"] * 1000.0
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path, help="ns-3 raw log file")
    parser.add_argument("--conn")
    parser.add_argument("--start-ms", type=float)
    parser.add_argument("--end-ms", type=float)
    parser.add_argument("--max-points", type=int, default=10000)
    parser.add_argument("--out-dir", type=Path, default=Path("figures_dctcp"))
    args = parser.parse_args()

    if not args.log.is_file():
        raise FileNotFoundError(args.log)

    alpha_rows, state_rows = parse_log(args.log)
    conn = choose_connection(alpha_rows, state_rows, args.conn)

    selected_alpha = sorted(
        [
            row
            for row in alpha_rows
            if in_range(row, conn, args.start_ms, args.end_ms)
        ],
        key=lambda row: row["time_s"],
    )

    selected_states = sorted(
        [
            row
            for row in state_rows
            if in_range(row, conn, args.start_ms, args.end_ms)
        ],
        key=lambda row: row["time_s"],
    )

    args.out_dir.mkdir(parents=True, exist_ok=True)

    if selected_alpha:
        write_csv(args.out_dir / "dctcp_alpha.csv", selected_alpha)
        plot_rows = thin_rows(selected_alpha, args.max_points)

        plt.figure(figsize=(11, 5.5))
        plt.step(
            [row["time_s"] * 1000.0 for row in plot_rows],
            [row["new_alpha"] for row in plot_rows],
            where="post",
            label="DCTCP alpha",
        )
        plt.step(
            [row["time_s"] * 1000.0 for row in plot_rows],
            [row["fraction"] for row in plot_rows],
            where="post",
            label="CE-marked fraction",
        )
        source_description = "DCTCP_ALPHA"
    else:
        if not selected_states:
            raise RuntimeError("No matching DCTCP records were found.")

        write_csv(args.out_dir / "dctcp_alpha_from_tcp_state.csv", selected_states)
        plot_rows = thin_rows(selected_states, args.max_points)

        plt.figure(figsize=(11, 5.5))
        plt.step(
            [row["time_s"] * 1000.0 for row in plot_rows],
            [row["alpha"] for row in plot_rows],
            where="post",
            label="DCTCP alpha",
        )
        source_description = "TCP_STATE fallback"

    plt.xlabel("Time (ms)")
    plt.ylabel("Ratio")
    plt.ylim(0, 1.05)
    plt.title(
        f"DCTCP congestion estimate\nConnection: {conn}"
    )
    plt.grid(True)
    plt.legend()
    plt.tight_layout()

    figure_path = args.out_dir / "dctcp_alpha.png"
    plt.savefig(figure_path, dpi=200)
    plt.close()

    # 额外输出 ECN 状态事件，便于检查 DCTCP 是否进入 ECE/CWR。
    if selected_states:
        event_rows = [
            row
            for row in selected_states
            if row["ecn_state"] in {"ECN_ECE_RCVD", "ECN_CWR_SENT"}
            or row["cong_state"] == "CA_CWR"
        ]
        write_csv(args.out_dir / "dctcp_ecn_events.csv", event_rows)

    print(f"Selected connection: {conn}")
    print(f"Alpha source: {source_description}")
    print(f"DCTCP_ALPHA records: {len(selected_alpha)}")
    print(f"TCP_STATE records: {len(selected_states)}")
    print(f"Figure: {figure_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())