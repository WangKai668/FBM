import numpy as np
import matplotlib.pyplot as plt
import os
import sys

def get_data(file_path):
    
    
    with open(file_path, 'r') as file:
        lines = file.readlines()
        if len(lines) <= 4:
            print("文件" + file_path + "缺乏数据")
            return
        
        ans = []
        for line in lines[4:]:
            parts = line.split('|')
            if len(parts) < 9:
                continue
            flag = int(parts[1].split('.')[-1][2:]) #取尾段数据
            
            if flag < 40000:
                continue
            #取txbytes和fct
            txbytes = int(parts[4])
            fct = float(parts[7])
            ans.append((txbytes, fct))
        return ans



def calculate_statistics_by_size(flow_data):
    # 定义每MB的字节数
    BYTES_PER_MB = 1024 * 1024
    
    bins = {
        "[0, 0.1)": (0, 0.1 * BYTES_PER_MB),
        "[0.1, 1)": (0.1 * BYTES_PER_MB, 1 * BYTES_PER_MB),
        "[1, 10)": (1 * BYTES_PER_MB, 10 * BYTES_PER_MB),
        "[10, +∞)": (10 * BYTES_PER_MB, float('inf'))
    }
    
    # 用于存储每个类别FCT值的字典
    bucketed_fcts = {
        "Overall": [],
        "[0, 0.1)": [],
        "[0.1, 1)": [],
        "[1, 10)": [],
        "[10, +∞)": []
    }

    for tx_bytes, fct in flow_data:
        # 将所有FCT值添加到"Overall"类别中
        bucketed_fcts["Overall"].append(fct)
        # 根据流量大小将FCT值分类到相应的区间
        for name, (lower, upper) in bins.items():
            if lower <= tx_bytes < upper:
                bucketed_fcts[name].append(fct)
                break
                
    # 计算统计数据的字典
    stats = {
        "avg": {},
        "p99": {}
    }
    
    # 遍历每个分类的FCT值
    for category, fcts in bucketed_fcts.items():
        if fcts:
            # 计算平均FCT值
            stats["avg"][category] = np.mean(fcts)
            # 计算99百分位FCT值
            stats["p99"][category] = np.percentile(fcts, 99)
        else:
            # 如果没有FCT值，则将平均值和99百分位值设为0
            stats["avg"][category] = 0
            stats["p99"][category] = 0
            
    return stats

def plot_charts(pbs_stats, bms_stats, id):
   
    categories_labels = ["Overall", "[0, 0.1)", "[0.1, 1)", "[1, 10)", "[10, +∞)"]
    x_axis_labels = ["Overall", "[0, 0.1)", "[0.1, 1)", "[1, 10)", "[10, +∞)"] 
    
    # --- 准备数据 ---
    pbs_avg = np.array([pbs_stats["avg"][cat] for cat in categories_labels])
    bms_avg = np.array([bms_stats["avg"][cat] for cat in categories_labels])
    pbs_p99 = np.array([pbs_stats["p99"][cat] for cat in categories_labels])
    bms_p99 = np.array([bms_stats["p99"][cat] for cat in categories_labels])
    
    # --- 归一化 (以bms为基准) ---
    with np.errstate(divide='ignore', invalid='ignore'):
        norm_pbs_avg = np.divide(pbs_avg, bms_avg)
        norm_pbs_p99 = np.divide(pbs_p99, bms_p99)


    bms_normalized = np.ones_like(bms_avg)

    # --- 绘图 ---
    plt.style.use('seaborn-v0_8-whitegrid')
    fig, axes = plt.subplots(1, 2, figsize=(10, 4), dpi=120)
    bar_width = 0.35
    index = np.arange(len(categories_labels))
    
    # 图 (a) Average FCT
    ax1 = axes[0]
    # 归一化的
    ax1.bar(index - bar_width/2, norm_pbs_avg, bar_width, label='pbs', 
            color='white', edgecolor='darkred', linewidth=1.5, hatch='////')
    ax1.bar(index + bar_width/2, bms_normalized, bar_width, label='bms', color='#2ca02c')
    
    # ax1.bar(index - bar_width/2, pbs_avg, bar_width, label='pbs', 
    #         color='white', edgecolor='darkred', linewidth=1.5, hatch='////')
    # ax1.bar(index + bar_width/2, bms_avg, bar_width, label='bms', color='#2ca02c')
    #ax1.set_ylim(0, 1.4)
    ax1.set_ylabel('Normalized FCT', fontsize=12, family='sans-serif')
    ax1.set_xlabel('Range of Flow Size (MB)', fontsize=12, family='sans-serif')
    ax1.set_title('(a) Average', fontsize=14, family='s ans-serif', pad=10)
    ax1.set_xticks(index)
    ax1.set_xticklabels(x_axis_labels)
    ax1.legend(loc='upper left', frameon=True, edgecolor='black', fancybox=False)
    
    # 图 (b) 99th Percentile FCT
    ax2 = axes[1]
    # 归一化
    ax2.bar(index - bar_width/2, norm_pbs_p99, bar_width, label='pbs', 
            color='white', edgecolor='darkred', linewidth=1.5, hatch='////')
    ax2.bar(index + bar_width/2, bms_normalized, bar_width, label='bms', color='#2ca02c')
    
    # ax2.bar(index - bar_width/2, pbs_p99, bar_width, label='pbs', 
    #         color='white', edgecolor='darkred', linewidth=1.5, hatch='////')
    # ax2.bar(index + bar_width/2, bms_p99, bar_width, label='bms', color='#2ca02c')

    ax2.set_ylabel('Normalized FCT', fontsize=12, family='sans-serif')
    ax2.set_xlabel('Range of Flow Size (MB)', fontsize=12, family='sans-serif')
    ax2.set_title('(b) 99th percentile', fontsize=14, family='sans-serif', pad=10)
    ax2.set_xticks(index)
    ax2.set_xticklabels(x_axis_labels)
    
    # 统一图例
    handles, labels = ax1.get_legend_handles_labels()
    # fig.legend(handles, labels, loc='upper center', ncol=2, frameon=True, edgecolor='black', fancybox=False)
    
    plt.tight_layout(rect=[0, 0, 1, 0.96]) # 调整布局为图例留出空间
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    save_path = base_dir + "/data-fig/compare/" + id + "/fct_comparison.png"

    save_path2 = base_dir + "/data-fig/compare/" + id + "/fct_comparison2.png"

    plt.savefig(save_path2)
    print("图表已保存为 fct_comparison.png")


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <test_id> (e.g. tc2-04)")
        sys.exit(1)

    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    id = sys.argv[1]
    type = "4.0M"
    file_bms = base_dir + "/data" + "/BMS" + "/" + id  + "/" + type +  "/flow-analysis-" + id + ".txt"
    file_bms = base_dir + "/data" + "/BMS" + "/" + id  + "/" + type +  "/flow-analysis-" + id + ".txt"
    file_pbs = base_dir + "/data" + "/pbs" + "/" + id + "/flow-analysis-" + id + ".txt"

    pbs_arr = get_data(file_pbs)
    bms_arr = get_data(file_bms)
    pbs_stat = calculate_statistics_by_size(pbs_arr)
    bms_stat = calculate_statistics_by_size(bms_arr)
    plot_charts(pbs_stat, bms_stat, id)


if __name__ == "__main__":
    main()