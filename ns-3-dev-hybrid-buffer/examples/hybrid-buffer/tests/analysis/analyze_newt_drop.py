#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
从日志文件中提取 port 0 的：

    1. 每行最前面的时间，单位 ns
    2. newT[T+1](ns)

然后识别连续下降阶段，并输出下降幅度最大的两个阶段。

运行：

    python3 analyze_newt_drop.py 日志文件路径

例如：

    python3 analyze_newt_drop.py \
    /home/sj/FBM1/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/pbs/tc2-05/hybrid-buffer-test-tc2-05.txt
"""

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List


# 支持整数、小数和科学计数法
NUMBER_PATTERN = r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?"


# 匹配示例：
#
# 76536 middle_value_for_plot: port: 0 ...
# newT[T+1](ns): 6773.65
LOG_PATTERN = re.compile(
    rf"^\s*(?P<time>{NUMBER_PATTERN})"
    rf"\s+middle_value_for_plot:"
    rf".*?\bport:\s*(?P<port>\d+)"
    rf".*?newT\[T\+1\]\(ns\):\s*(?P<new_t>{NUMBER_PATTERN})"
)


def parse_port0_newt(log_file: Path) -> List[Dict]:
    """
    提取 port 0 的时间和 newT。

    返回：

        [
            {
                "time_ns": 76536.0,
                "new_t_ns": 6773.65,
                "line_number": 3
            },
            ...
        ]
    """

    if not log_file.is_file():
        raise FileNotFoundError(
            f"日志文件不存在：{log_file}"
        )

    records = []

    with log_file.open(
        "r",
        encoding="utf-8",
        errors="ignore",
    ) as file:

        for line_number, line in enumerate(
            file,
            start=1,
        ):
            match = LOG_PATTERN.search(line)

            if match is None:
                continue

            port = int(match.group("port"))

            # 只处理 port 0
            if port != 0:
                continue

            time_ns = float(match.group("time"))
            new_t_ns = float(match.group("new_t"))

            records.append(
                {
                    "time_ns": time_ns,
                    "new_t_ns": new_t_ns,
                    "line_number": line_number,
                }
            )

    # 按照时间排序
    records.sort(
        key=lambda item: item["time_ns"]
    )

    return records


def find_decreasing_stages(
    records: List[Dict],
    min_points: int = 2,
) -> List[Dict]:
    """
    查找连续下降阶段。

    例如 newT：

        16000, 14000, 10000, 8000, 9000

    则下降阶段为：

        16000 -> 8000

    当后一个 newT 不再小于前一个 newT 时，
    当前下降阶段结束。
    """

    if len(records) < 2:
        return []

    stages = []

    start_index = None

    for index in range(1, len(records)):

        previous_value = records[index - 1]["new_t_ns"]
        current_value = records[index]["new_t_ns"]

        # 当前值小于前一个值，说明正在下降
        if current_value < previous_value:

            if start_index is None:
                start_index = index - 1

        else:
            # 当前值没有下降，结束之前的下降阶段
            if start_index is not None:
                end_index = index - 1

                add_stage(
                    records=records,
                    stages=stages,
                    start_index=start_index,
                    end_index=end_index,
                    min_points=min_points,
                )

                start_index = None

    # 如果最后一段一直下降到文件末尾
    if start_index is not None:
        add_stage(
            records=records,
            stages=stages,
            start_index=start_index,
            end_index=len(records) - 1,
            min_points=min_points,
        )

    return stages


def add_stage(
    records: List[Dict],
    stages: List[Dict],
    start_index: int,
    end_index: int,
    min_points: int,
) -> None:
    """生成一个下降阶段并加入结果。"""

    point_count = end_index - start_index + 1

    if point_count < min_points:
        return

    start_record = records[start_index]
    end_record = records[end_index]

    drop_ns = (
        start_record["new_t_ns"]
        - end_record["new_t_ns"]
    )

    # 必须确实发生下降
    if drop_ns <= 0:
        return

    duration_ns = (
        end_record["time_ns"]
        - start_record["time_ns"]
    )

    stages.append(
        {
            "start_time_ns": start_record["time_ns"],
            "end_time_ns": end_record["time_ns"],
            "start_new_t_ns": start_record["new_t_ns"],
            "end_new_t_ns": end_record["new_t_ns"],
            "drop_ns": drop_ns,
            "duration_ns": duration_ns,
            "point_count": point_count,
        }
    )


def print_extracted_data(records: List[Dict]) -> None:
    """打印提取到的数据基本信息。"""

    print()
    print("=" * 78)
    print("port 0 的 newT 数据提取结果")
    print("=" * 78)

    print(f"数据点数量：{len(records)}")

    if not records:
        return

    print(
        "时间范围："
        f"{records[0]['time_ns']:.3f} ns"
        " -> "
        f"{records[-1]['time_ns']:.3f} ns"
    )

    print(
        "时间范围："
        f"{records[0]['time_ns'] / 1_000_000:.9f} ms"
        " -> "
        f"{records[-1]['time_ns'] / 1_000_000:.9f} ms"
    )


def print_top_two_stages(stages: List[Dict]) -> None:
    """
    按总下降量从大到小选择两个阶段，
    最终按照时间先后打印。
    """

    if not stages:
        print()
        print("没有检测到连续下降阶段。")
        return

    # 按下降量从大到小选择前两个
    top_stages = sorted(
        stages,
        key=lambda item: item["drop_ns"],
        reverse=True,
    )[:2]

    # 输出时按照发生时间排序
    top_stages.sort(
        key=lambda item: item["start_time_ns"]
    )

    print()
    print("=" * 78)
    print("newT 下降最明显的两个时间阶段")
    print("=" * 78)

    for index, stage in enumerate(
        top_stages,
        start=1,
    ):
        start_time_ns = stage["start_time_ns"]
        end_time_ns = stage["end_time_ns"]

        start_time_ms = start_time_ns / 1_000_000
        end_time_ms = end_time_ns / 1_000_000

        duration_us = stage["duration_ns"] / 1000

        start_new_t_us = stage["start_new_t_ns"] / 1000
        end_new_t_us = stage["end_new_t_ns"] / 1000
        drop_us = stage["drop_ns"] / 1000

        print(f"下降阶段 {index}：")

        print(
            "  原始时间："
            f"{start_time_ns:.3f} ns"
            " -> "
            f"{end_time_ns:.3f} ns"
        )

        print(
            "  毫秒时间："
            f"{start_time_ms:.9f} ms"
            " -> "
            f"{end_time_ms:.9f} ms"
        )

        print(
            f"  持续时间：{duration_us:.6f} us"
        )

        print(
            "  newT变化："
            f"{stage['start_new_t_ns']:.6f} ns"
            " -> "
            f"{stage['end_new_t_ns']:.6f} ns"
        )

        print(
            "  newT变化："
            f"{start_new_t_us:.6f} us"
            " -> "
            f"{end_new_t_us:.6f} us"
        )

        print(
            f"  总下降量：{drop_us:.6f} us"
        )

        print(
            f"  数据点数：{stage['point_count']}"
        )

        if index != len(top_stages):
            print("-" * 78)

    print("=" * 78)


def print_all_stages(stages: List[Dict]) -> None:
    """简要打印所有检测到的下降阶段。"""

    print()
    print(f"共检测到 {len(stages)} 个连续下降阶段：")

    for index, stage in enumerate(
        stages,
        start=1,
    ):
        print(
            f"  阶段 {index}: "
            f"{stage['start_time_ns'] / 1_000_000:.9f} ms"
            " -> "
            f"{stage['end_time_ns'] / 1_000_000:.9f} ms，"
            f"下降 {stage['drop_ns'] / 1000:.6f} us"
        )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "提取日志中 port 0 的 newT[T+1](ns)，"
            "并输出下降最明显的两个时间阶段"
        )
    )

    parser.add_argument(
        "log_file",
        type=Path,
        help="hybrid-buffer-test-*.txt 日志文件路径",
    )

    parser.add_argument(
        "--min-points",
        type=int,
        default=2,
        help=(
            "一个下降阶段至少包含的数据点数量，"
            "默认是2"
        ),
    )

    return parser.parse_args()


def main() -> int:
    args = parse_arguments()

    try:
        records = parse_port0_newt(
            args.log_file
        )

        if not records:
            print(
                "错误：没有找到 port 0 的 "
                "middle_value_for_plot 数据。",
                file=sys.stderr,
            )
            return 1

        stages = find_decreasing_stages(
            records,
            min_points=args.min_points,
        )

        print_extracted_data(
            records
        )

        print_all_stages(
            stages
        )

        print_top_two_stages(
            stages
        )

    except (
        OSError,
        ValueError,
    ) as exc:
        print(
            f"错误：{exc}",
            file=sys.stderr,
        )
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())