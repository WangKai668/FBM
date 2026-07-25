#!/usr/bin/env python3

import os
import re
import sys
import pandas as pd


# 当前脚本目录：tests/analysis
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# tests/data
DATA_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "data"))

BMS_THRESHOLDS = ["0.2M", "0.5M", "1.0M", "2.0M", "4.0M"]

LABELS = [
    "BMS-0.2M",
    "BMS-0.5M",
    "BMS-1.0M",
    "BMS-2.0M",
    "BMS-4.0M",
    "PBS"
]


def calculate_fct_metrics(input_file):
    """
    读取flow-analysis文件，计算全部正向数据流的：
    1. 平均FCT
    2. P95 FCT
    3. P99 FCT

    返回值单位：毫秒
    """

    if not os.path.exists(input_file):
        print(f"警告：文件不存在：{input_file}", file=sys.stderr)
        return None

    try:
        # 文件前4行分别是标题、分隔线、列名、分隔线
        df = pd.read_csv(
            input_file,
            sep="|",
            skiprows=4,
            header=None,
            engine="python"
        )
    except Exception as e:
        print(f"警告：读取文件失败：{input_file}，原因：{e}", file=sys.stderr)
        return None

    if df.empty:
        print(f"警告：文件中没有流数据：{input_file}", file=sys.stderr)
        return None

    # 删除由分隔符产生的全空列
    df = df.dropna(axis=1, how="all")

    if len(df.columns) != 11:
        print(
            f"警告：文件列数错误，期望11列，实际{len(df.columns)}列：{input_file}",
            file=sys.stderr
        )
        return None

    df.columns = [
        "FlowID",
        "Source",
        "Destination",
        "Proto",
        "TxBytes",
        "RxBytes",
        "TxPkts",
        "RxPkts",
        "Loss",
        "FCT(s)",
        "Throughput(Mbps)"
    ]

    # 清除字符串两端空格
    for column in df.columns:
        if df[column].dtype == "object":
            df[column] = df[column].str.strip()

    # 提取源端口
    # 正向数据流的源端口通常大于40000
    # ACK反向流的源端口通常是服务器端口，例如100
    df["SourcePort"] = pd.to_numeric(
        df["Source"].str.extract(r":(\d+)$")[0],
        errors="coerce"
    )

    # 只保留正向数据流
    df = df[df["SourcePort"] > 40000].copy()

    # 转换FCT为数值
    df["FCT(s)"] = pd.to_numeric(df["FCT(s)"], errors="coerce")
    df = df.dropna(subset=["FCT(s)"])

    if df.empty:
        print(f"警告：没有找到有效的正向数据流：{input_file}", file=sys.stderr)
        return None

    # 秒转换为毫秒
    fct_ms = df["FCT(s)"] * 1000

    return {
        "avg": fct_ms.mean(),
        "p95": fct_ms.quantile(0.95),
        "p99": fct_ms.quantile(0.99),
        "flow_count": len(df)
    }


def build_analysis_path(case_name, algorithm, threshold=None, flow_rate=None):
    """构建flow-analysis文件路径"""

    if algorithm == "BMS":
        path_parts = [
            DATA_DIR,
            "BMS",
            case_name,
            threshold
        ]
    else:
        path_parts = [
            DATA_DIR,
            "pbs",
            case_name
        ]

    # tc2-08等实验存在100、200……900速率子目录
    if flow_rate is not None:
        path_parts.append(str(flow_rate))

    path_parts.append(f"flow-analysis-{case_name}.txt")

    return os.path.join(*path_parts)


def find_flow_rates(case_name):
    """
    自动判断实验是否存在速率子目录。

    普通实验返回：
        [None]

    tc2-08这种实验返回：
        [100, 200, ..., 900]
    """

    pbs_case_dir = os.path.join(DATA_DIR, "pbs", case_name)

    direct_pbs_file = os.path.join(
        pbs_case_dir,
        f"flow-analysis-{case_name}.txt"
    )

    # 普通目录结构
    if os.path.exists(direct_pbs_file):
        return [None]

    flow_rates = set()

    # 从PBS目录查找速率
    if os.path.isdir(pbs_case_dir):
        for name in os.listdir(pbs_case_dir):
            full_path = os.path.join(pbs_case_dir, name)

            if os.path.isdir(full_path) and re.fullmatch(r"\d+", name):
                flow_rates.add(int(name))

    # 如果PBS目录没找到，再从BMS目录查找
    if not flow_rates:
        for threshold in BMS_THRESHOLDS:
            threshold_dir = os.path.join(
                DATA_DIR,
                "BMS",
                case_name,
                threshold
            )

            if not os.path.isdir(threshold_dir):
                continue

            for name in os.listdir(threshold_dir):
                full_path = os.path.join(threshold_dir, name)

                if os.path.isdir(full_path) and re.fullmatch(r"\d+", name):
                    flow_rates.add(int(name))

    if flow_rates:
        return sorted(flow_rates)

    # 没找到速率目录时，按普通目录尝试
    return [None]


def collect_all_metrics(case_name, flow_rate=None):
    """读取5个BMS阈值和1个PBS结果"""

    result_list = []

    # BMS的五个阈值
    for threshold in BMS_THRESHOLDS:
        input_file = build_analysis_path(
            case_name=case_name,
            algorithm="BMS",
            threshold=threshold,
            flow_rate=flow_rate
        )

        result_list.append(calculate_fct_metrics(input_file))

    # PBS
    pbs_file = build_analysis_path(
        case_name=case_name,
        algorithm="pbs",
        flow_rate=flow_rate
    )

    result_list.append(calculate_fct_metrics(pbs_file))

    return result_list


def format_values(results, metric_name):
    """将指标格式化为列表"""

    values = []

    for result in results:
        if result is None:
            values.append("NaN")
        else:
            values.append(f"{result[metric_name]:.3f}")

    return "[" + ", ".join(values) + "]"


def print_metrics(results):
    """按照指定格式输出结果"""

    label_line = ", ".join(LABELS)

    print("Avg FCT:")
    print(label_line)
    print(format_values(results, "avg"))

    print("P95 FCT:")
    print(label_line)
    print(format_values(results, "p95"))

    print("P99 FCT:")
    print(label_line)
    print(format_values(results, "p99"))


def main():
    if len(sys.argv) != 2:
        print(f"用法：python3 {sys.argv[0]} <实验名字>")
        print(f"示例：python3 {sys.argv[0]} tc2-03-h-40b")
        print(f"示例：python3 {sys.argv[0]} tc2-08")
        sys.exit(1)

    case_name = sys.argv[1]

    flow_rates = find_flow_rates(case_name)

    for index, flow_rate in enumerate(flow_rates):
        if index > 0:
            print()

        if flow_rate is not None:
            print(f"Flow Rate: {flow_rate} Gbps")

        results = collect_all_metrics(
            case_name=case_name,
            flow_rate=flow_rate
        )

        print_metrics(results)


if __name__ == "__main__":
    main()