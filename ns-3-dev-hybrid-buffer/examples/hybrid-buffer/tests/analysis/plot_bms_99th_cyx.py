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

def plot_charts(bms_stats_list, id):
    type_list = ['DeepHir-0.2M', 'DeepHir-0.5M', 'DeepHir-1.0M', 
                 'DeepHir-2.0M', 'DeepHir-4.0M', 'FBM']  
    # 流量大小区间类别
    categories_labels = ["Overall", "[0, 0.1)", "[0.1, 1)", "[1, 10)", "[10, +∞)"]
    x_axis_labels = ["Overall", "[0, 0.1)", "[0.1, 1)", "[1, 10)", "[10, +∞)"] 
    bar_width = 0.15  

    plt.style.use('seaborn-v0_8-whitegrid')
    fig, ax = plt.subplots(figsize=(10, 6), dpi=120)
    index = np.arange(len(categories_labels))

    for i, bms_stats in enumerate(bms_stats_list):
        p99_data = np.array([bms_stats["p99"][cat] for cat in categories_labels])
        offset = (i - 2.5) * bar_width  
        ax.bar(index + offset, p99_data, width=bar_width, label=type_list[i], 
               color=plt.cm.Set1(i), edgecolor='black')  

    ax.set_ylabel('99th percentile FCT(s)', fontsize=12, family='sans-serif')
    ax.set_xlabel('Range of Flow Size (MB)', fontsize=12, family='sans-serif')
    ax.set_title('(b) 99th percentile', fontsize=14, family='sans-serif', pad=10)
    ax.set_xticks(index)
    ax.set_xticklabels(x_axis_labels)
    ax.legend(loc='upper left', frameon=True, edgecolor='black', fancybox=False)

    plt.tight_layout()
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    save_path = os.path.join(base_dir, "data-fig", "BMS", id, "99th_fct_comparison.png")
    os.makedirs(os.path.dirname(save_path), exist_ok=True)  
    plt.savefig(save_path)
    print(f"图表已保存为 {save_path}")


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <test_id> (e.g. tc2-04)")
        sys.exit(1)

    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    id = sys.argv[1]
    type_list = ['0.2M', '0.5M', '1.0M', '2.0M', '4.0M']

    arr = []
    for i in range(0 , 5):

        type = type_list[i]
        file_bms = base_dir + "/data" + "/BMS" + "/" + id  + "/" + type +  "/flow-analysis-" + id + ".txt"

        bms_arr = get_data(file_bms)
        bms_stat = calculate_statistics_by_size(bms_arr)
        arr.append(bms_stat)

    file_pbs = base_dir + "/data" + "/pbs" + "/" + id   +  "/flow-analysis-" + id + ".txt"
    pbs_arr = get_data(file_pbs)
    pbs_stat = calculate_statistics_by_size(pbs_arr)
    arr.append(pbs_stat)

    plot_charts(arr, id)


if __name__ == "__main__":
    main()