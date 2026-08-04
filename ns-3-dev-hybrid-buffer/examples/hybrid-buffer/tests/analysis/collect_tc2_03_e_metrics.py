#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
扫描 tests/data/pbs/ 下所有形如 tc2-03-<数值>e 的目录，
分别读取：
    1. flow-analysis-<目录名>.txt
    2. loss_packet.csv

输出每个目录的：
    - Avg FCT（ms）
    - Packet Loss
    - 正向流数量

默认结果保存为：
    tests/data/pbs/tc2-03-e-metrics.csv

使用：
    python3 collect_tc2_03_e_metrics.py

也可以指定前缀：
    python3 collect_tc2_03_e_metrics.py tc2-03
"""

import csv
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional

import pandas as pd


# 当前脚本一般放在 tests/analysis
SCRIPT_DIR = Path(__file__).resolve().parent
TESTS_DIR = SCRIPT_DIR.parent
PBS_DIR = TESTS_DIR / "data" / "pbs"


def read_last_csv_field(csv_path: Path) -> float:
    """读取 CSV 最后一条非空记录的最后一列。"""
    if not csv_path.is_file():
        raise FileNotFoundError(f"丢包文件不存在：{csv_path}")

    last_row = None

    with csv_path.open("r", encoding="utf-8", errors="replace", newline="") as file:
        reader = csv.reader(file)

        for row in reader:
            if row and any(field.strip() for field in row):
                last_row = row

    if last_row is None:
        raise ValueError(f"丢包文件为空：{csv_path}")

    try:
        return float(last_row[-1].strip())
    except ValueError as exc:
        raise ValueError(
            f"丢包文件最后一列不是数字：{csv_path}，内容={last_row[-1]!r}"
        ) from exc


def calculate_avg_fct_ms(flow_analysis_path: Path) -> Optional[dict]:
    """
    读取 flow-analysis 文件，计算正向数据流平均 FCT。

    正向流判定：
        Source 字段最后的源端口 > 40000

    返回：
        {
            "avg_fct_ms": ...,
            "flow_count": ...
        }
    """
    if not flow_analysis_path.is_file():
        print(f"警告：FCT文件不存在：{flow_analysis_path}", file=sys.stderr)
        return None

    try:
        dataframe = pd.read_csv(
            flow_analysis_path,
            sep="|",
            skiprows=4,
            header=None,
            engine="python",
        )
    except Exception as exc:
        print(
            f"警告：读取FCT文件失败：{flow_analysis_path}，原因：{exc}",
            file=sys.stderr,
        )
        return None

    if dataframe.empty:
        print(f"警告：FCT文件没有数据：{flow_analysis_path}", file=sys.stderr)
        return None

    # 删除 | 分隔产生的空列
    dataframe = dataframe.dropna(axis=1, how="all")

    if len(dataframe.columns) != 11:
        print(
            f"警告：列数不正确，期望11列，实际{len(dataframe.columns)}列："
            f"{flow_analysis_path}",
            file=sys.stderr,
        )
        return None

    dataframe.columns = [
        "FlowID",
        "Source",
        "Destination",
        "Proto",
        "TxBytes",
        "RxBytes",
        "TxPkts",
        "RxPkts",
        "Loss",
        "FCT_s",
        "Throughput_Mbps",
    ]

    # 清除字符串字段两侧空格
    for column in dataframe.columns:
        if dataframe[column].dtype == "object":
            dataframe[column] = dataframe[column].astype(str).str.strip()

    # 提取 Source 最后的端口号
    dataframe["SourcePort"] = pd.to_numeric(
        dataframe["Source"].str.extract(r":(\d+)$")[0],
        errors="coerce",
    )

    # 仅保留正向数据流
    dataframe = dataframe[dataframe["SourcePort"] > 40000].copy()

    dataframe["FCT_s"] = pd.to_numeric(
        dataframe["FCT_s"],
        errors="coerce",
    )

    dataframe = dataframe.dropna(subset=["FCT_s"])

    if dataframe.empty:
        print(
            f"警告：没有找到有效正向数据流：{flow_analysis_path}",
            file=sys.stderr,
        )
        return None

    fct_ms = dataframe["FCT_s"] * 1000.0

    return {
        "avg_fct_ms": float(fct_ms.mean()),
        "flow_count": int(len(dataframe)),
    }


def extract_e_value(directory_name: str, prefix: str) -> Optional[float]:
    """
    从目录名中提取 e 前面的数值。

    例如：
        tc2-03-0.01e -> 0.01
        tc2-03-0.5e  -> 0.5
        tc2-03-1e    -> 1.0
    """
    pattern = rf"^{re.escape(prefix)}-(\d+(?:\.\d+)?)e$"
    match = re.fullmatch(pattern, directory_name)

    if match is None:
        return None

    return float(match.group(1))


def find_flow_analysis_file(case_dir: Path) -> Optional[Path]:
    """
    优先读取：
        flow-analysis-<目录名>.txt

    若不存在，则尝试目录内唯一的 flow-analysis-*.txt。
    """
    expected = case_dir / f"flow-analysis-{case_dir.name}.txt"

    if expected.is_file():
        return expected

    candidates = sorted(case_dir.glob("flow-analysis-*.txt"))

    if len(candidates) == 1:
        return candidates[0]

    if not candidates:
        return None

    print(
        f"警告：目录中存在多个 flow-analysis 文件，默认使用第一个："
        f"{candidates[0]}",
        file=sys.stderr,
    )
    return candidates[0]


def collect_metrics(prefix: str) -> List[Dict]:
    """扫描 PBS 目录并按 e 数值从小到大收集指标。"""
    if not PBS_DIR.is_dir():
        raise FileNotFoundError(f"PBS目录不存在：{PBS_DIR}")

    matched_directories = []

    for entry in PBS_DIR.iterdir():
        if not entry.is_dir():
            continue

        e_value = extract_e_value(entry.name, prefix)

        if e_value is not None:
            matched_directories.append((e_value, entry))

    matched_directories.sort(key=lambda item: item[0])

    if not matched_directories:
        raise RuntimeError(
            f"没有找到形如 {prefix}-<数值>e 的目录，扫描位置：{PBS_DIR}"
        )

    results = []

    for e_value, case_dir in matched_directories:
        print(f"正在处理目录：{case_dir.name}")

        flow_file = find_flow_analysis_file(case_dir)
        loss_file = case_dir / "loss_packet.csv"

        fct_result = (
            calculate_avg_fct_ms(flow_file)
            if flow_file is not None
            else None
        )

        try:
            packet_loss = read_last_csv_field(loss_file)
        except (FileNotFoundError, ValueError) as exc:
            print(f"警告：{exc}", file=sys.stderr)
            packet_loss = None

        results.append(
            {
                "directory": case_dir.name,
                "e_value": e_value,
                "avg_fct_ms": (
                    fct_result["avg_fct_ms"]
                    if fct_result is not None
                    else None
                ),
                "packet_loss": packet_loss,
                "flow_count": (
                    fct_result["flow_count"]
                    if fct_result is not None
                    else None
                ),
                "flow_analysis_file": (
                    str(flow_file)
                    if flow_file is not None
                    else ""
                ),
                "loss_file": str(loss_file),
            }
        )

    return results


def save_results(results: List[Dict], prefix: str) -> Path:
    """将汇总结果保存到 PBS 根目录。"""
    output_path = PBS_DIR / f"{prefix}-e-metrics.csv"

    fieldnames = [
        "directory",
        "e_value",
        "avg_fct_ms",
        "packet_loss",
        "flow_count",
        "flow_analysis_file",
        "loss_file",
    ]

    with output_path.open(
        "w",
        encoding="utf-8",
        newline="",
    ) as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()

        for result in results:
            row = result.copy()

            if row["avg_fct_ms"] is not None:
                row["avg_fct_ms"] = f'{row["avg_fct_ms"]:.6f}'

            if row["packet_loss"] is not None:
                row["packet_loss"] = f'{row["packet_loss"]:.0f}'

            writer.writerow(row)

    return output_path

def print_results(results: List[Dict]) -> None:
    """按目录打印结果。"""
    print()
    print("=" * 86)
    print(
        f'{"Directory":<20}'
        f'{"e":>10}'
        f'{"Avg FCT (ms)":>20}'
        f'{"Packet Loss":>18}'
        f'{"Flow Count":>16}'
    )
    print("-" * 86)

    for result in results:
        avg_fct_text = (
            f'{result["avg_fct_ms"]:.6f}'
            if result["avg_fct_ms"] is not None
            else "NaN"
        )

        loss_text = (
            f'{result["packet_loss"]:.0f}'
            if result["packet_loss"] is not None
            else "NaN"
        )

        flow_count_text = (
            str(result["flow_count"])
            if result["flow_count"] is not None
            else "NaN"
        )

        print(
            f'{result["directory"]:<20}'
            f'{result["e_value"]:>10g}'
            f'{avg_fct_text:>20}'
            f'{loss_text:>18}'
            f'{flow_count_text:>16}'
        )

    print("=" * 86)


def main() -> int:
    prefix = sys.argv[1] if len(sys.argv) > 1 else "tc2-03"

    try:
        results = collect_metrics(prefix)
        print_results(results)

        output_path = save_results(results, prefix)

        print()
        print(f"汇总文件已保存：{output_path}")

    except (OSError, RuntimeError, ValueError) as exc:
        print(f"错误：{exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())