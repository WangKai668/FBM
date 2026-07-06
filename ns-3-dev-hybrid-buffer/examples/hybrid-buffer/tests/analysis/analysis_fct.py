#!/usr/bin/env python3
import pandas as pd
import os
import numpy as np

# 当前Python脚本所在目录：tests/analysis
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# tests目录
TESTS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))

# tests/data目录
DATA_DIR = os.path.join(TESTS_DIR, "data")

def process_flow_data(input_file):
    # 判断输入文件类型（BMS或PBS）
    if 'BMS' in input_file:
        data_type = 'BMS'
    elif 'pbs' in input_file:
        data_type = 'PBS'
    else:
        raise ValueError("无法确定输入文件类型，路径中应包含'BMS'或'pbs'")
    
    # 读取文件，跳过前3行(标题和分隔线)
    df = pd.read_csv(input_file, sep='|', skiprows=4, header=None)
    
    # 清理数据 - 去除每列前后的空格
    df = df.apply(lambda x: x.str.strip() if x.dtype == "object" else x)
    
    # 删除空列(由分隔符|产生的)
    df = df.dropna(axis=1, how='all')
    
    # 设置列名（根据新格式）
    df.columns = [
        'FlowID', 'Source', 'Destination', 'Proto', 
        'TxBytes', 'RxBytes', 'TxPkts', 'RxPkts', 'Loss', 'FCT(s)', 'Throughput(Mbps)'
    ]
    
    # 提取源端口号
    df['SourcePort'] = df['Source'].str.extract(r':(\d+)$').astype(int)
    
    # 过滤数据 - 只保留源端口号大于40000的流
    df_filtered = df[df['SourcePort'] > 40000].copy()
    
    # 转换数值列
    numeric_cols = ['TxBytes', 'RxBytes', 'TxPkts', 'RxPkts', 'FCT(s)', 'Throughput(Mbps)']
    df_filtered[numeric_cols] = df_filtered[numeric_cols].apply(pd.to_numeric, errors='coerce')
    
    # 重新计算丢失包数（txPkts - rxPkts）
    df_filtered['Loss'] = df_filtered['TxPkts'] - df_filtered['RxPkts']
    
    # 按RxBytes从小到大排序
    df_sorted = df_filtered.sort_values(by='RxBytes', ascending=True)
    
    # 删除临时列
    df_sorted = df_sorted.drop(columns=['SourcePort'])
    
    # 划分大流和小流（以100个包为分界线）
    large_flows = df_sorted[df_sorted['TxPkts'] >= 10]
    small_flows = df_sorted[df_sorted['TxPkts'] < 10]
    
    # 计算汇总统计信息（整体）
    total_tx_bytes = df_sorted['TxBytes'].sum()
    total_rx_bytes = df_sorted['RxBytes'].sum()
    total_tx_pkts = df_sorted['TxPkts'].sum()
    total_rx_pkts = df_sorted['RxPkts'].sum()
    total_loss = total_tx_pkts - total_rx_pkts
    avg_fct = df_sorted['FCT(s)'].mean() * 1000  # 转换为毫秒
    p95_fct = df_sorted['FCT(s)'].quantile(0.95) * 1000
    p99_fct = df_sorted['FCT(s)'].quantile(0.99) * 1000
    
    # 计算大流统计信息
    large_tx_pkts = large_flows['TxPkts'].sum()
    large_rx_pkts = large_flows['RxPkts'].sum()
    large_loss = large_tx_pkts - large_rx_pkts
    large_avg_fct = large_flows['FCT(s)'].mean() * 1000
    large_p95_fct = large_flows['FCT(s)'].quantile(0.95) * 1000
    large_p99_fct = large_flows['FCT(s)'].quantile(0.99) * 1000
    
    # 计算小流统计信息
    small_tx_pkts = small_flows['TxPkts'].sum()
    small_rx_pkts = small_flows['RxPkts'].sum()
    small_loss = small_tx_pkts - small_rx_pkts
    small_avg_fct = small_flows['FCT(s)'].mean() * 1000
    small_p95_fct = small_flows['FCT(s)'].quantile(0.95) * 1000
    small_p99_fct = small_flows['FCT(s)'].quantile(0.99) * 1000
    
    # 准备输出文件名
    output_file = os.path.join(
        SCRIPT_DIR,
        f"filtered_and_sorted_flows_{data_type}.txt"
    )
    
    # 构建汇总统计行
    with open(output_file, 'w') as f:
        # 写入整体统计信息
        f.write("Overall Statistics:\n")
        f.write(f"Total Flows: {len(df_sorted)} (Large: {len(large_flows)}, Small: {len(small_flows)})\n")
        f.write(f"Total TxBytes: {total_tx_bytes:,} | RxBytes: {total_rx_bytes:,}\n")
        f.write(f"Total TxPkts: {total_tx_pkts:,} | RxPkts: {total_rx_pkts:,} | Loss: {total_loss:,}\n")
        f.write(f"Avg FCT: {avg_fct:.3f}ms | P95 FCT: {p95_fct:.3f}ms | P99 FCT: {p99_fct:.3f}ms\n")
        f.write("-"*150 + "\n")
        
        # 写入大流统计信息
        f.write("\nLarge Flows Statistics (TxPkts >= 10):\n")
        f.write(f"Count: {len(large_flows)} | TxPkts: {large_tx_pkts:,} | RxPkts: {large_rx_pkts:,} | Loss: {large_loss:,}\n")
        f.write(f"Avg FCT: {large_avg_fct:.3f}ms | P95 FCT: {large_p95_fct:.3f}ms | P99 FCT: {large_p99_fct:.3f}ms\n")
        f.write("-"*150 + "\n")
        
        # 写入小流统计信息
        f.write("\nSmall Flows Statistics (TxPkts < 10):\n")
        f.write(f"Count: {len(small_flows)} | TxPkts: {small_tx_pkts:,} | RxPkts: {small_rx_pkts:,} | Loss: {small_loss:,}\n")
        f.write(f"Avg FCT: {small_avg_fct:.3f}ms | P95 FCT: {small_p95_fct:.3f}ms | P99 FCT: {small_p99_fct:.3f}ms\n")
        f.write("-"*150 + "\n")
        
        # 写入详细流信息
        f.write("\nFlow Performance Analysis (sorted by RxBytes ascending):\n")
        f.write("-"*150 + "\n")
        f.write("FlowID | Source               | Destination          | Proto  | TxBytes    | RxBytes    | TxPkts   | RxPkts   | Loss   | FCT(ms)     | Throughput(Mbps)\n")
        f.write("-"*150 + "\n")
        
        for _, row in df_sorted.iterrows():
            flow_type = "L" if row['TxPkts'] >= 100 else "S"
            f.write(
                f"{flow_type}{row['FlowID']:5} | {row['Source']:20} | {row['Destination']:20} | "
                f"{row['Proto']:6} | {row['TxBytes']:10} | {row['RxBytes']:10} | "
                f"{row['TxPkts']:8} | {row['RxPkts']:8} | "
                f"{row['Loss']:6} | {row['FCT(s)']*1000:10.3f} | {row['Throughput(Mbps)']:10.2f}\n"
            )

    print(f"处理完成，{data_type}结果已保存到 {output_file}")


case_name = "tc2-04"

input_files = [
os.path.join(
        DATA_DIR,
        "BMS",
        case_name,
        "2.0M",
        f"flow-analysis-{case_name}.txt"
    ),
    os.path.join(
        DATA_DIR,
        "pbs",
        case_name,
        f"flow-analysis-{case_name}.txt"
    )
]
for input_file in input_files:
    print(f"准备处理文件：{input_file}")

    try:
        process_flow_data(input_file)
    except Exception as e:
        print(f"处理文件 {input_file} 时出错: {str(e)}")
