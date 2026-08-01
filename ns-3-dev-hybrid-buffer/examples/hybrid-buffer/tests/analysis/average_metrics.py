#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
将多次实验的统计结果直接粘贴到 DATA_GROUPS 中。

脚本会按照实验目录名匹配，对以下指标计算多次实验均值：

    1. Avg FCT
    2. Packet Loss

运行：

    python3 average_metrics.py
"""

import re
from collections import defaultdict
from typing import Dict, List


# ============================================================
# 1. 将5组实验数据粘贴到这里
# ============================================================

DATA_GROUPS = [
    # ========================
    # 第1组实验数据
    # ========================
    r"""
在这里粘贴第1组数据
例如：

tc2-03-0.001e          0.001            1.292927             10851           14789
tc2-03-0.01e            0.01            1.421426             11545           14789
tc2-03-0.1e              0.1            1.145082              8926           14789
tc2-03-1e                  1            1.376969             11180           14789
tc2-03-10e                10            1.349028             10732           14789

tc2-03-2R                  2            1.312607             10693           14789
tc2-03-4R                  4            1.297499             10439           14789
tc2-03-8R                  8            1.181699              9450           14789
tc2-03-16R                16            1.145082              8926           14789
tc2-03-32R                32            1.309625             10170           14789
""",

    # ========================
    # 第2组实验数据
    # ========================
    r"""
在这里粘贴第2组数据
tc2-03-0.001e            0.001            1.081645              8111           14789
tc2-03-0.01e              0.01            1.144762              8587           14789
tc2-03-0.1e                0.1            1.112541              8760           14789
tc2-03-1e                    1            1.300121             10557           14789
tc2-03-10e                  10            1.239294              9731           14789

tc2-03-2R                    2            1.219592              9756           14789
tc2-03-4R                    4            1.247911              9601           14789
tc2-03-8R                    8            1.270183             10074           14789
tc2-03-16R                  16            1.112541              8760           14789
tc2-03-32R                  32            1.181224              9834           14789
""",

    # ========================
    # 第3组实验数据
    # ========================
    r"""
在这里粘贴第3组数据
tc2-03-0.001e            0.001            1.551310             12805           14789
tc2-03-0.01e              0.01            1.348070             10759           14789
tc2-03-0.1e                0.1            1.633869             12511           14789
tc2-03-1e                    1            1.488736             12179           14789
tc2-03-10e                  10            1.436731             11035           14789

tc2-03-2R                    2            1.353926             11486           14789
tc2-03-4R                    4            1.329912             10583           14789
tc2-03-8R                    8            1.349002             10825           14789
tc2-03-16R                  16            1.633869             12511           14789
tc2-03-32R                  32            1.518682             12267           14789
""",

    # ========================
    # 第4组实验数据
    # ========================
    r"""
在这里粘贴第4组数据
tc2-03-0.001e            0.001            1.321911             10670           14789
tc2-03-0.01e              0.01            1.497091             11902           14789
tc2-03-0.1e                0.1            1.306881             10189           14789
tc2-03-1e                    1            1.446866             11058           14789
tc2-03-10e                  10            1.368829             10768           14789

tc2-03-2R                    2            1.138682              9310           14789
tc2-03-4R                    4            1.457832             11988           14789
tc2-03-8R                    8            1.526894             11984           14789
tc2-03-16R                  16            1.306881             10189           14789
tc2-03-32R                  32            1.363072             10745           14789
""",

    # ========================
    # 第5组实验数据
    # ========================
    r"""
在这里粘贴第5组数据
tc2-03-0.001e            0.001            1.242989              9641           14789
tc2-03-0.01e              0.01            1.085572              8703           14789
tc2-03-0.1e                0.1            1.232325             10078           14789
tc2-03-1e                    1            1.179658              9323           14789
tc2-03-10e                  10            0.957274              7586           14789

tc2-03-2R                    2            1.109850              9149           14789
tc2-03-4R                    4            1.094206              8527           14789
tc2-03-8R                    8            1.196352              9266           14789
tc2-03-16R                  16            1.232325             10078           14789
tc2-03-32R                  32            1.061793              8622           14789
""",
]


# ============================================================
# 2. 数据解析
# ============================================================

def normalize_experiment_name(name: str) -> str:
    """
    统一实验名称，避免 R/r 大小写导致无法匹配。

    例如：

        tc2-03-2R
        tc2-03-2r

    都会被视为同一个实验。
    """
    return name.strip().lower()


def parse_data_group(text: str) -> Dict[str, dict]:
    """
    解析一组实验数据。

    期望每条数据格式为：

        实验目录 参数 AvgFCT PacketLoss FlowCount

    例如：

        tc2-03-0.001e  0.001  1.292927  10851  14789

    表头、空行、分隔线和无关文字会自动跳过。
    """

    results = {}

    for line_number, raw_line in enumerate(
        text.splitlines(),
        start=1,
    ):
        line = raw_line.strip()

        if not line:
            continue

        # 只处理以 tc 开头的数据行
        if not line.lower().startswith("tc"):
            continue

        fields = re.split(r"\s+", line)

        # 至少需要：
        # directory、参数、avg_fct、packet_loss
        if len(fields) < 4:
            print(
                f"警告：第 {line_number} 行字段不足，已跳过：{line}"
            )
            continue

        experiment_name = fields[0]

        try:
            parameter = float(fields[1])
            avg_fct_ms = float(fields[2])
            packet_loss = float(fields[3])

            flow_count = None

            if len(fields) >= 5:
                flow_count = int(float(fields[4]))

        except ValueError:
            print(
                f"警告：第 {line_number} 行存在非数字字段，"
                f"已跳过：{line}"
            )
            continue

        normalized_name = normalize_experiment_name(
            experiment_name
        )

        if normalized_name in results:
            print(
                f"警告：同一组数据中实验重复：{experiment_name}，"
                "后面的记录将覆盖前面的记录"
            )

        results[normalized_name] = {
            "directory": experiment_name,
            "parameter": parameter,
            "avg_fct_ms": avg_fct_ms,
            "packet_loss": packet_loss,
            "flow_count": flow_count,
        }

    return results


# ============================================================
# 3. 汇总多组实验
# ============================================================

def collect_all_groups(
    data_groups: List[str],
) -> Dict[str, dict]:
    """
    收集所有实验组，并按照实验目录名汇总。
    """

    collected = defaultdict(
        lambda: {
            "display_name": "",
            "parameter": None,
            "avg_fct_values": [],
            "packet_loss_values": [],
            "flow_count_values": [],
            "group_indexes": [],
        }
    )

    for group_index, group_text in enumerate(
        data_groups,
        start=1,
    ):
        group_results = parse_data_group(
            group_text
        )

        if not group_results:
            print(
                f"警告：第 {group_index} 组没有解析到有效数据"
            )
            continue

        for normalized_name, result in group_results.items():
            item = collected[normalized_name]

            if not item["display_name"]:
                item["display_name"] = result["directory"]

            if item["parameter"] is None:
                item["parameter"] = result["parameter"]

            item["avg_fct_values"].append(
                result["avg_fct_ms"]
            )

            item["packet_loss_values"].append(
                result["packet_loss"]
            )

            if result["flow_count"] is not None:
                item["flow_count_values"].append(
                    result["flow_count"]
                )

            item["group_indexes"].append(
                group_index
            )

    return dict(collected)


# ============================================================
# 4. 排序
# ============================================================

def experiment_sort_key(item):
    """
    按实验类型和参数排序。

    排序顺序：

        e实验
        r实验
        其他实验
    """

    normalized_name, result = item
    parameter = result["parameter"]

    if normalized_name.endswith("e"):
        experiment_type = 0
    elif normalized_name.endswith("r"):
        experiment_type = 1
    else:
        experiment_type = 2

    return (
        experiment_type,
        parameter if parameter is not None else float("inf"),
        normalized_name,
    )


# ============================================================
# 5. 计算平均值
# ============================================================

def calculate_mean(values: List[float]) -> float:
    """计算算术平均值。"""

    if not values:
        return float("nan")

    return sum(values) / len(values)


# ============================================================
# 6. 输出终端表格
# ============================================================

def print_average_results(
    collected: Dict[str, dict],
    expected_group_count: int,
) -> None:
    """
    输出每个实验的平均结果。
    """

    print()
    print("=" * 105)

    print(
        f'{"Directory":<24}'
        f'{"Parameter":>12}'
        f'{"Avg FCT Mean (ms)":>22}'
        f'{"Packet Loss Mean":>22}'
        f'{"Samples":>12}'
        f'{"Status":>13}'
    )

    print("-" * 105)

    sorted_items = sorted(
        collected.items(),
        key=experiment_sort_key,
    )

    for _, result in sorted_items:
        avg_fct_mean = calculate_mean(
            result["avg_fct_values"]
        )

        packet_loss_mean = calculate_mean(
            result["packet_loss_values"]
        )

        sample_count = len(
            result["avg_fct_values"]
        )

        if sample_count == expected_group_count:
            status = "OK"
        else:
            status = (
                f"缺{expected_group_count - sample_count}组"
            )

        print(
            f'{result["display_name"]:<24}'
            f'{result["parameter"]:>12g}'
            f'{avg_fct_mean:>22.6f}'
            f'{packet_loss_mean:>22.2f}'
            f'{sample_count:>12}'
            f'{status:>13}'
        )

    print("=" * 105)


# ============================================================
# 7. 输出方便复制的一行数组
# ============================================================

def print_copyable_results(
    collected: Dict[str, dict],
) -> None:
    """
    输出可直接复制到绘图代码中的数据。
    """

    sorted_items = sorted(
        collected.items(),
        key=experiment_sort_key,
    )

    experiment_names = [
        result["display_name"]
        for _, result in sorted_items
    ]

    avg_fct_means = [
        calculate_mean(
            result["avg_fct_values"]
        )
        for _, result in sorted_items
    ]

    packet_loss_means = [
        calculate_mean(
            result["packet_loss_values"]
        )
        for _, result in sorted_items
    ]

    print()
    print("方便复制的数据：")
    print()

    print("Experiment Names:")
    print(
        "["
        + ", ".join(
            f'"{name}"'
            for name in experiment_names
        )
        + "]"
    )

    print()
    print("Avg FCT Mean:")
    print(
        "["
        + ", ".join(
            f"{value:.6f}"
            for value in avg_fct_means
        )
        + "]"
    )

    print()
    print("Packet Loss Mean:")
    print(
        "["
        + ", ".join(
            f"{value:.2f}"
            for value in packet_loss_means
        )
        + "]"
    )


# ============================================================
# 8. 分别输出 e 和 R 两类数据
# ============================================================

def print_results_by_type(
    collected: Dict[str, dict],
) -> None:
    """
    分别输出 e 实验和 R 实验，便于直接复制。
    """

    groups = {
        "e": [],
        "r": [],
    }

    for normalized_name, result in collected.items():
        if normalized_name.endswith("e"):
            groups["e"].append(
                (normalized_name, result)
            )

        elif normalized_name.endswith("r"):
            groups["r"].append(
                (normalized_name, result)
            )

    for experiment_type, title in [
        ("e", "E experiments"),
        ("r", "R experiments"),
    ]:
        items = sorted(
            groups[experiment_type],
            key=experiment_sort_key,
        )

        if not items:
            continue

        labels = [
            result["display_name"]
            for _, result in items
        ]

        parameters = [
            result["parameter"]
            for _, result in items
        ]

        avg_values = [
            calculate_mean(
                result["avg_fct_values"]
            )
            for _, result in items
        ]

        loss_values = [
            calculate_mean(
                result["packet_loss_values"]
            )
            for _, result in items
        ]

        print()
        print(title + ":")
        print(
            ", ".join(labels)
        )

        print("Parameters:")
        print(
            "["
            + ", ".join(
                f"{value:g}"
                for value in parameters
            )
            + "]"
        )

        print("Avg FCT:")
        print(
            "["
            + ", ".join(
                f"{value:.6f}"
                for value in avg_values
            )
            + "]"
        )

        print("Packet Loss:")
        print(
            "["
            + ", ".join(
                f"{value:.2f}"
                for value in loss_values
            )
            + "]"
        )


# ============================================================
# 9. 主函数
# ============================================================

def main() -> None:
    expected_group_count = len(DATA_GROUPS)

    collected = collect_all_groups(
        DATA_GROUPS
    )

    if not collected:
        print(
            "错误：没有解析到任何有效实验数据。"
        )
        return

    print_average_results(
        collected,
        expected_group_count,
    )

    print_copyable_results(
        collected
    )

    print_results_by_type(
        collected
    )


if __name__ == "__main__":
    main()