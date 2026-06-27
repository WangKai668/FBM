import numpy as np
import matplotlib.pyplot as plt
import os
import sys
from matplotlib.pyplot import MultipleLocator 

final_font_size=26
plt.rc('font',family='Times New Roman')

def get_data(file_path):
    
    with open(file_path, 'r') as file:
        lines = file.readlines()
        if len(lines) <= 4:
            print("文件" + file_path + "缺乏数据")
            return
        
        ans = []
        for line in lines[4:]:
            parts = line.split('|')
            
            # if len(parts) < 11:
            #     continue
            flag = int(parts[1].split('.')[-1][2:]) #取尾段数据

            parts = [part.strip() for part in parts]
            
            if flag < 40000:
                continue
            #取txbytes和fct
            #txbytes = int(parts[4])
            # print(len(parts))
            fct = float(parts[-2])
            ans.append(fct)
        return ans


def plot_fct_avg(avg):
    if len(avg) != 4 or any(len(group) != 6 for group in avg):
        print("error: 数据维度需为 4 组")
        return
    

    data = []
    for i in range(0,6):
        tmp = []
        for j in range(0,4):
            tmp.append(avg[j][i])
        data.append(tmp)
    
    fig=plt.figure(dpi=600,figsize=(12, 8))
    ax=fig.add_subplot(111)

    plt.tick_params(axis='x', labelsize=final_font_size)  # 设置x轴的字体大小
    plt.tick_params(axis='y', labelsize=final_font_size)  # 设置y轴的字体大小


    labels=['WebSearch','Hadoop','WebSearch\n+Incast','Hadoop\n+Incast']
    x = np.arange(len(labels))  # x轴刻度标签位置
    width = 0.12  # 柱子的宽度
    colors_BMS = ['green','blue','purple','k','orange','r']
    # 计算每个柱子在x轴上的位置，保证x轴刻度标签居中
    plt.bar(x - 2.5*width, data[0], width, label='D-0.2M', color='green', alpha=0.7, hatch='/', edgecolor='white')
    plt.bar(x - 1.5*width, data[1], width, label='D-0.5M', color='blue', alpha=0.7, hatch='\\', edgecolor='white')
    plt.bar(x - 0.5*width, data[2], width, label='D-1.0M', color='purple', alpha=0.7, hatch='x', edgecolor='white')
    plt.bar(x + 0.5*width, data[3], width, label='D-2.0M', color='k', alpha=0.7, hatch='o', edgecolor='white')
    plt.bar(x + 1.5*width, data[4], width, label='D-4.0M', color='orange', alpha=0.7, hatch='+', edgecolor='white')
    plt.bar(x + 2.5*width, data[5], width, label='FBM', color='r', alpha=0.7, hatch='*', edgecolor='white')
    plt.ylabel('FCT(s)',fontsize=final_font_size)
    
    plt.xticks(x, labels=labels)
    plt.yscale("log")
    plt.legend(bbox_to_anchor=(-0.08, 1.001), loc=3, borderaxespad=0,labels=["DeepHir-0.2M","DeepHir-0.5M","DeepHir-1M","DeepHir-2M","DeepHir-4M","FBM"],ncol=3,fontsize=final_font_size,columnspacing=1.3)
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(bottom=0.15,left=0.15,top=0.8)

    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    save_dir = os.path.join(base_dir, "data-fig", "compare")
    os.makedirs(save_dir, exist_ok=True)
    save_path = os.path.join(save_dir, "all_tc_fct_avg.pdf")
    plt.savefig(save_path, bbox_inches='tight', dpi=300)
    plt.close()

def plot_fct_p99(avg):
    if len(avg) != 4 or any(len(group) != 6 for group in avg):
        print("error: 数据维度需为 4 组")
        return
    

    data = []
    for i in range(0,6):
        tmp = []
        for j in range(0,4):
            tmp.append(avg[j][i])
        data.append(tmp)
    
    fig=plt.figure(dpi=600,figsize=(12, 8))
    ax=fig.add_subplot(111)

    plt.tick_params(axis='x', labelsize=final_font_size)  # 设置x轴的字体大小
    plt.tick_params(axis='y', labelsize=final_font_size)  # 设置y轴的字体大小


    labels=['WebSearch','Hadoop','WebSearch\n+Incast','Hadoop\n+Incast']
    x = np.arange(len(labels))  # x轴刻度标签位置
    width = 0.12  # 柱子的宽度
    colors_BMS = ['green','blue','purple','k','orange','r']
    # 计算每个柱子在x轴上的位置，保证x轴刻度标签居中
    plt.bar(x - 2.5*width, data[0], width, label='D-0.2M', color='green', alpha=0.7, hatch='/', edgecolor='white')
    plt.bar(x - 1.5*width, data[1], width, label='D-0.5M', color='blue', alpha=0.7, hatch='\\', edgecolor='white')
    plt.bar(x - 0.5*width, data[2], width, label='D-1.0M', color='purple', alpha=0.7, hatch='x', edgecolor='white')
    plt.bar(x + 0.5*width, data[3], width, label='D-2.0M', color='k', alpha=0.7, hatch='o', edgecolor='white')
    plt.bar(x + 1.5*width, data[4], width, label='D-4.0M', color='orange', alpha=0.7, hatch='+', edgecolor='white')
    plt.bar(x + 2.5*width, data[5], width, label='FBM', color='r', alpha=0.7, hatch='*', edgecolor='white')
    plt.ylabel('FCT(s)',fontsize=final_font_size)
    
    plt.xticks(x, labels=labels)
    plt.yscale("log")
    plt.legend(bbox_to_anchor=(-0.08, 1.001), loc=3, borderaxespad=0,labels=["DeepHir-0.2M","DeepHir-0.5M","DeepHir-1M","DeepHir-2M","DeepHir-4M","FBM"],ncol=3,fontsize=final_font_size,columnspacing=1.3)
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(bottom=0.15,left=0.15,top=0.8)

    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    save_dir = os.path.join(base_dir, "data-fig", "compare")
    os.makedirs(save_dir, exist_ok=True)
    save_path = os.path.join(save_dir, "all_tc_fct_p99.pdf")
    plt.savefig(save_path, bbox_inches='tight', dpi=300)
    plt.close()

def main():

    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    id_list = ['tc2-01', 'tc2-02', 'tc2-03', 'tc2-04']
    type_list = ['0.2M', '0.5M', '1.0M', '2.0M', '4.0M']
    
    avg = []
    p99 = []

    for i in range(0, 4):
        id = id_list[i]
        tc_avg = []
        tc_p99 = []
        for j in range(0, 5):
            type = type_list[j]
            file_bms = base_dir + "/data" + "/BMS" + "/" + id + "/" + type + "/flow-analysis-" + id + ".txt"
            
            if not os.path.exists(file_bms):
                print(f"Error: txt file not found at {file_bms}")
                continue
            bms_arr = get_data(file_bms)

            #print("BMS/" + id + "/" + type + "(avg)" + ":" + str(np.mean(bms_arr)))
            tc_avg.append(np.mean(bms_arr))
            #print("BMS/" + id + "/" + type + "(99th)" + ":" + str(np.percentile(bms_arr, 99)))
            tc_p99.append(np.percentile(bms_arr, 99))

        file_pbs = base_dir + "/data" + "/pbs" + "/" + id + "/flow-analysis-" + id + ".txt"

        if not os.path.exists(file_pbs):
            print(f"Error: txt file not found at {file_pbs}")
            continue

        pbs_arr = get_data(file_pbs)
        #print("pbs/" + id  + "(avg)" + ":" + str(np.mean(pbs_arr)))
        tc_avg.append(np.mean(pbs_arr))

        #print("pbs/" + id  + "(99th)" + ":" + str(np.percentile(pbs_arr, 99)))
        tc_p99.append(np.percentile(pbs_arr, 99))

        avg.append(tc_avg)
        p99.append(tc_p99)


    plot_fct_avg(avg)
    plot_fct_p99(p99)


if __name__ == "__main__":
    main()