import matplotlib.pyplot as plt
import re

def parse_tcp_data(filename):
    """解析TCP数据文件，提取时间、CWND、SSTHRESH和InFlight数据"""
    times, cwnds, ssthreshs, inflights = [], [], [], []
    
    with open(filename, 'r') as file:
        for line in file:
            try:
                # 使用单个正则表达式匹配所有字段（更高效）
                match = re.search(
                    r'TCP State - (\d+\.\d+e?-?\d*).*?'
                    r'CWND: (\d+) bytes.*?'
                    r'SSTHRESH: (\d+) bytes.*?'
                    r'InFlight: (\d+) bytes',
                    line
                )
                if match:
                    time = float(match.group(1))
                    cwnd = int(match.group(2)) / 1500  # 转换为数据包数（假设MTU=1500）
                    ssthresh = int(match.group(3)) / 1500
                    inflight = int(match.group(4)) / 1500
                    
                    times.append(time)
                    cwnds.append(cwnd)
                    ssthreshs.append(ssthresh)
                    inflights.append(inflight)
            except (ValueError, IndexError) as e:
                print(f"解析行时出错: {line.strip()} | 错误: {e}")
    
    print(f"解析完成: {len(times)} 数据点")
    return times, cwnds, ssthreshs, inflights

def plot_tcp_data(times, cwnds, ssthreshs, inflights):
    """绘制TCP指标随时间变化图"""
    plt.figure(figsize=(12, 6))
    
    # 转换时间单位（秒→毫秒）并计算相对时间
    start_time = times[0]
    times_ms = [(t - start_time) * 1000 for t in times]
    
    # 绘制曲线
    plt.plot(times_ms, cwnds, label='CWND (pkts)', color='blue', linewidth=2)
    plt.plot(times_ms, inflights, label='InFlight (pkts)', color='green', linewidth=1.5)
    
    # 仅当SSTHRESH不是固定值时绘制（示例数据中均为4294967295）
    if len(set(ssthreshs)) > 1:
        plt.plot(times_ms, ssthreshs, label='SSTHRESH (pkts)', color='red', linestyle='--')
    
    # 图表美化
    # plt.xlim(0, 5)  # 聚焦前5ms
    plt.xlabel('Time since start (ms)')
    plt.ylabel('Packets (MTU=1500B)')
    plt.title('TCP Congestion Window Dynamics')
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.5)
    
    # 自动调整Y轴范围（排除异常值）
    y_max = max(max(cwnds), max(inflights)) * 1.1
    plt.ylim(0, y_max)
    
    plt.tight_layout()
    plt.savefig('tcp_metrics.png', dpi=300, bbox_inches='tight')
    plt.show()

if __name__ == "__main__":
    input_file = 'filtered_output.txt'
    try:
        times, cwnds, ssthreshs, inflights = parse_tcp_data(input_file)
        if times:  # 确保有数据
            plot_tcp_data(times, cwnds, ssthreshs, inflights)
        else:
            print("错误: 未解析到有效数据！")
    except FileNotFoundError:
        print(f"错误: 文件 {input_file} 未找到！")