#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
扫描 tests/data/pbs/ 下所有形如 tc2-03-<数值>r 的目录。

每个目录读取：

    1. flow-analysis-<目录名>.txt
    2. loss_packet.csv

统计：

    - Avg FCT（ms）
    - 95th FCT（ms）
    - Packet Loss
    - 正向流数量

正向流筛选条件：

    Source 字段中的源端口号 > 40000

默认输出：

    tests/data/pbs/tc2-03-r-metrics.csv
    tests/data/pbs/tc2-03-r-metrics-copy.txt

运行方式：

    python3 collect_tc2_03_r_metrics.py

也可以指定测试前缀：

    python3 collect_tc2_03_r_metrics.py tc2-03
"""

import csv
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional

import pandas as pd


# ============================================================
# 1. 路径配置
# ============================================================

# 当前脚本通常位于：
# tests/analysis/collect_tc2_03_r_metrics.py

SCRIPT_DIR = Path(__file__).resolve().parent
TESTS_DIR = SCRIPT_DIR.parent
PBS_DIR = TESTS_DIR / "data" / "pbs"


# ============================================================
# 2. 读取丢包数量
# ============================================================

def read_last_csv_field(csv_path: Path) -> float:
    """
    读取 CSV 文件最后一条非空记录的最后一列。

    例如：

        time,packet_loss
        0.01,10693

    返回：

        10693
    """

    if not csv_path.is_file():
        raise FileNotFoundError(
            f"丢包文件不存在：{csv_path}"
        )

    last_row = None

    with csv_path.open(
        "r",
        encoding="utf-8",
        errors="replace",
        newline="",
    ) as file:

        reader = csv.reader(file)

        for row in reader:
            if row and any(
                field.strip()
                for field in row
            ):
                last_row = row

    if last_row is None:
        raise ValueError(
            f"丢包文件为空：{csv_path}"
        )

    last_field = last_row[-1].strip()

    try:
        return float(last_field)

    except ValueError as exc:
        raise ValueError(
            f"丢包文件最后一列不是数字："
            f"{csv_path}，内容={last_field!r}"
        ) from exc


# ============================================================
# 3. 计算 Avg FCT 和 95th FCT
# ============================================================

def calculate_fct_metrics(
    flow_analysis_path: Path,
) -> Optional[Dict]:
    """
    读取 flow-analysis 文件并计算正向流的 FCT 指标。

    正向流判定：

        Source 字段中的源端口号 > 40000

    返回：

        {
            "avg_fct_ms": 平均FCT，单位ms,
            "p95_fct_ms": 95th FCT，单位ms,
            "flow_count": 正向流数量
        }
    """

    if not flow_analysis_path.is_file():
        print(
            f"警告：FCT文件不存在：{flow_analysis_path}",
            file=sys.stderr,
        )
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
            f"警告：读取FCT文件失败："
            f"{flow_analysis_path}，原因：{exc}",
            file=sys.stderr,
        )
        return None

    if dataframe.empty:
        print(
            f"警告：FCT文件没有数据："
            f"{flow_analysis_path}",
            file=sys.stderr,
        )
        return None

    # 删除由 | 分隔符产生的全空列
    dataframe = dataframe.dropna(
        axis=1,
        how="all",
    )

    if len(dataframe.columns) != 11:
        print(
            f"警告：列数不正确，期望11列，"
            f"实际{len(dataframe.columns)}列："
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

    # 清除字符串字段两侧的空格
    for column in dataframe.columns:
        if dataframe[column].dtype == "object":
            dataframe[column] = (
                dataframe[column]
                .astype(str)
                .str.strip()
            )

    # 提取 Source 字段最后的端口号
    #
    # 例如：
    #
    #     10.1.1.2:49153
    #
    # 提取：
    #
    #     49153

    dataframe["SourcePort"] = pd.to_numeric(
        dataframe["Source"].str.extract(
            r":(\d+)$"
        )[0],
        errors="coerce",
    )

    # 只保留正向数据流
    dataframe = dataframe[
        dataframe["SourcePort"] > 40000
    ].copy()

    # 将 FCT 转换为数值
    dataframe["FCT_s"] = pd.to_numeric(
        dataframe["FCT_s"],
        errors="coerce",
    )

    # 删除无效 FCT
    dataframe = dataframe.dropna(
        subset=["FCT_s"]
    )

    if dataframe.empty:
        print(
            f"警告：没有找到有效正向数据流："
            f"{flow_analysis_path}",
            file=sys.stderr,
        )
        return None

    # 秒转换为毫秒
    fct_ms = dataframe["FCT_s"] * 1000.0

    return {
        "avg_fct_ms": float(
            fct_ms.mean()
        ),

        "p95_fct_ms": float(
            fct_ms.quantile(0.95)
        ),

        "flow_count": int(
            len(dataframe)
        ),
    }


# ============================================================
# 4. 提取目录名中的 r 数值
# ============================================================

def extract_r_value(
    directory_name: str,
    prefix: str,
) -> Optional[float]:
    """
    从目录名中提取 r 前面的数值。

    例如：

        tc2-03-2r    -> 2.0
        tc2-03-4r    -> 4.0
        tc2-03-8r    -> 8.0
        tc2-03-0.5r  -> 0.5
        tc2-03-32r   -> 32.0
    """

    pattern = (
        rf"^{re.escape(prefix)}-"
        rf"(\d+(?:\.\d+)?)r$"
    )

    match = re.fullmatch(
        pattern,
        directory_name,
    )

    if match is None:
        return None

    return float(
        match.group(1)
    )


# ============================================================
# 5. 查找 flow-analysis 文件
# ============================================================

def find_flow_analysis_file(
    case_dir: Path,
) -> Optional[Path]:
    """
    优先查找：

        flow-analysis-<目录名>.txt

    例如：

        tc2-03-2r/
        flow-analysis-tc2-03-2r.txt

    如果预期文件不存在，则查找目录内的：

        flow-analysis-*.txt
    """

    expected_file = (
        case_dir
        / f"flow-analysis-{case_dir.name}.txt"
    )

    if expected_file.is_file():
        return expected_file

    candidates = sorted(
        case_dir.glob("flow-analysis-*.txt")
    )

    if len(candidates) == 1:
        return candidates[0]

    if not candidates:
        return None

    print(
        "警告：目录中存在多个 flow-analysis 文件，"
        f"默认使用第一个：{candidates[0]}",
        file=sys.stderr,
    )

    return candidates[0]


# ============================================================
# 6. 扫描并收集所有实验结果
# ============================================================

def collect_metrics(
    prefix: str,
) -> List[Dict]:
    """
    扫描 PBS_DIR 下所有形如：

        <prefix>-<数值>r

    的目录，并按照 r 数值从小到大排序。
    """

    if not PBS_DIR.is_dir():
        raise FileNotFoundError(
            f"PBS目录不存在：{PBS_DIR}"
        )

    matched_directories = []

    for entry in PBS_DIR.iterdir():

        if not entry.is_dir():
            continue

        r_value = extract_r_value(
            entry.name,
            prefix,
        )

        if r_value is not None:
            matched_directories.append(
                (r_value, entry)
            )

    # 按照 r 数值从小到大排序
    matched_directories.sort(
        key=lambda item: item[0]
    )

    if not matched_directories:
        raise RuntimeError(
            f"没有找到形如 "
            f"{prefix}-<数值>r 的目录，"
            f"扫描位置：{PBS_DIR}"
        )

    results = []

    for r_value, case_dir in matched_directories:

        print(
            f"正在处理目录：{case_dir.name}"
        )

        flow_file = find_flow_analysis_file(
            case_dir
        )

        loss_file = (
            case_dir
            / "loss_packet.csv"
        )

        # 统计 FCT
        if flow_file is not None:
            fct_result = calculate_fct_metrics(
                flow_file
            )

        else:
            print(
                f"警告：目录中没有找到 "
                f"flow-analysis 文件：{case_dir}",
                file=sys.stderr,
            )
            fct_result = None

        # 统计丢包
        try:
            packet_loss = read_last_csv_field(
                loss_file
            )

        except (
            FileNotFoundError,
            ValueError,
        ) as exc:

            print(
                f"警告：{exc}",
                file=sys.stderr,
            )

            packet_loss = None

        results.append(
            {
                "directory": case_dir.name,
                "r_value": r_value,

                "avg_fct_ms": (
                    fct_result["avg_fct_ms"]
                    if fct_result is not None
                    else None
                ),

                "p95_fct_ms": (
                    fct_result["p95_fct_ms"]
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

                "loss_file": str(
                    loss_file
                ),
            }
        )

    return results


# ============================================================
# 7. 格式化输出值
# ============================================================

def format_r_label(
    r_value: float,
) -> str:
    """
    将 r 数值转换为标签。

    例如：

        2.0 -> r=2
        0.5 -> r=0.5
    """

    return f"r={r_value:g}"


def format_fct_value(
    value: Optional[float],
) -> str:
    """
    FCT 保留 6 位小数。
    """

    if value is None:
        return "NaN"

    return f"{value:.6f}"


def format_loss_value(
    value: Optional[float],
) -> str:
    """
    丢包数量以整数形式输出。
    """

    if value is None:
        return "NaN"

    return f"{value:.0f}"


def format_flow_count(
    value: Optional[int],
) -> str:
    """
    流数量以整数形式输出。
    """

    if value is None:
        return "NaN"

    return str(value)


# ============================================================
# 8. 保存 CSV 结果
# ============================================================

def save_csv_results(
    results: List[Dict],
    prefix: str,
) -> Path:
    """
    将详细统计结果保存为 CSV。
    """

    output_path = (
        PBS_DIR
        / f"{prefix}-r-metrics.csv"
    )

    fieldnames = [
        "directory",
        "r_value",
        "avg_fct_ms",
        "p95_fct_ms",
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

        writer = csv.DictWriter(
            file,
            fieldnames=fieldnames,
        )

        writer.writeheader()

        for result in results:

            row = result.copy()

            if row["avg_fct_ms"] is not None:
                row["avg_fct_ms"] = (
                    f'{row["avg_fct_ms"]:.6f}'
                )

            if row["p95_fct_ms"] is not None:
                row["p95_fct_ms"] = (
                    f'{row["p95_fct_ms"]:.6f}'
                )

            if row["packet_loss"] is not None:
                row["packet_loss"] = (
                    f'{row["packet_loss"]:.0f}'
                )

            writer.writerow(row)

    return output_path


# ============================================================
# 9. 构造方便复制的数组格式
# ============================================================

def build_copyable_text(
    results: List[Dict],
) -> str:
    """
    构造方便复制到 Python 绘图代码中的一行数组格式。
    """

    labels = ", ".join(
        format_r_label(
            result["r_value"]
        )
        for result in results
    )

    r_values = ", ".join(
        f'{result["r_value"]:g}'
        for result in results
    )

    avg_fct_values = ", ".join(
        format_fct_value(
            result["avg_fct_ms"]
        )
        for result in results
    )

    p95_fct_values = ", ".join(
        format_fct_value(
            result["p95_fct_ms"]
        )
        for result in results
    )

    packet_loss_values = ", ".join(
        format_loss_value(
            result["packet_loss"]
        )
        for result in results
    )

    flow_count_values = ", ".join(
        format_flow_count(
            result["flow_count"]
        )
        for result in results
    )

    lines = [
        "R values:",
        labels,
        f"[{r_values}]",
        "",

        "Avg FCT:",
        labels,
        f"[{avg_fct_values}]",
        "",

        "95th FCT:",
        labels,
        f"[{p95_fct_values}]",
        "",

        "Packet Loss:",
        labels,
        f"[{packet_loss_values}]",
        "",

        "Flow Count:",
        labels,
        f"[{flow_count_values}]",
    ]

    return "\n".join(lines)


# ============================================================
# 10. 保存方便复制的 TXT 文件
# ============================================================

def save_copyable_results(
    results: List[Dict],
    prefix: str,
) -> Path:
    """
    将方便复制的数组结果保存为 TXT。
    """

    output_path = (
        PBS_DIR
        / f"{prefix}-r-metrics-copy.txt"
    )

    output_path.write_text(
        build_copyable_text(results) + "\n",
        encoding="utf-8",
    )

    return output_path


# ============================================================
# 11. 打印详细表格
# ============================================================

def print_table(
    results: List[Dict],
) -> None:
    """
    在终端中打印详细结果表格。
    """

    line_width = 108

    print()
    print("=" * line_width)

    print(
        f'{"Directory":<20}'
        f'{"r":>10}'
        f'{"Avg FCT (ms)":>20}'
        f'{"95th FCT (ms)":>20}'
        f'{"Packet Loss":>18}'
        f'{"Flow Count":>16}'
    )

    print("-" * line_width)

    for result in results:

        avg_fct_text = format_fct_value(
            result["avg_fct_ms"]
        )

        p95_fct_text = format_fct_value(
            result["p95_fct_ms"]
        )

        loss_text = format_loss_value(
            result["packet_loss"]
        )

        flow_count_text = format_flow_count(
            result["flow_count"]
        )

        print(
            f'{result["directory"]:<20}'
            f'{result["r_value"]:>10g}'
            f'{avg_fct_text:>20}'
            f'{p95_fct_text:>20}'
            f'{loss_text:>18}'
            f'{flow_count_text:>16}'
        )

    print("=" * line_width)


# ============================================================
# 12. 打印方便复制的数组
# ============================================================

def print_copyable_results(
    results: List[Dict],
) -> None:
    """
    在终端中打印可直接复制的数据。
    """

    print()
    print("=" * 86)
    print("方便复制的数据：")
    print("=" * 86)
    print()

    print(
        build_copyable_text(results)
    )

    print()
    print("=" * 86)


# ============================================================
# 13. 主函数
# ============================================================

def main() -> int:
    """
    默认扫描 tc2-03。

    运行：

        python3 collect_tc2_03_r_metrics.py

    指定其他前缀：

        python3 collect_tc2_03_r_metrics.py tc2-04
    """

    prefix = (
        sys.argv[1]
        if len(sys.argv) > 1
        else "tc2-03"
    )

    try:
        # 收集所有指标
        results = collect_metrics(
            prefix
        )

        # 打印详细表格
        print_table(
            results
        )

        # 打印方便复制的数组
        print_copyable_results(
            results
        )

        # 保存 CSV
        csv_output_path = save_csv_results(
            results,
            prefix,
        )

        # 保存 TXT
        txt_output_path = save_copyable_results(
            results,
            prefix,
        )

        print()
        print(
            f"CSV汇总文件已保存："
            f"{csv_output_path}"
        )

        print(
            f"复制格式文件已保存："
            f"{txt_output_path}"
        )

    except (
        OSError,
        RuntimeError,
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