#coding:utf-8
import os
import numpy as np
import csv
import re
import sys
import random
import bisect
import matplotlib.pyplot as plt
from matplotlib.pyplot import MultipleLocator 
from mpl_toolkits.axes_grid1 import host_subplot
from mpl_toolkits import axisartist

# 当前脚本所在目录：tests/analysis
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# tests目录
TESTS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))

# tests/data目录
data_dir = os.path.join(TESTS_DIR, "data") + os.sep

# tests/data-fig目录
save_path = os.path.join(TESTS_DIR, "data-fig") + os.sep

final_font_size=26
plt.rc('font',family='Times New Roman')

def get_last_field(file_name):
    with open(file_name, 'rb') as f:
        f.seek(-2, 2)  # 移动到倒数第二个字节
        while f.read(1) != b'\n':  # 读到换行符为止
            f.seek(-2, 1)  # 继续向前移动
        last_line = f.readline().decode()  # 读取最后一行
    return last_line.strip().split(',')[-2]  # 获取最后第二项

def get_mean(file_name):
    with open(file_name, 'r') as file: 
        reader = csv.reader(file)
        next(reader)
        throughput = [int(row[-1]) for row in reader]
        for i in throughput:
            if i >258746294770:
                print(file_name+"  "+"xzxzx:" +str(i))
                break
        mean = sum(throughput) / len(throughput)
        return mean

def calculate_decrease(data):
    decreases = []
    last_value = data[-1]
    for i in range(5):
        decrease = (data[i] - last_value) / data[i]
        decreases.append(decrease)
    return decreases

def test9_plot():
    print("画图ing")

    # x_data = ['D-0.2M','D-1.0M','D-2.0M','D-3.0M','D-4.0M','FBM']
    x_data = ['D-0.2M', 'D-0.5M','D-1.0M','D-2.0M','D-4.0M','FBM']
    #Infocom版本
    # ax_data = [2410080.0, 2411210.0, 2400590.0, 2395250.0, 2403300.0, 2110080.0] #websearch
    # ax1_data = [2221780.0, 2220170.0, 2218010.0, 2214740.0, 2208180.0, 1895990.0] #fbhdp
    # ax2_data = [2587440.0, 2578450.0, 2573780.0, 2560670.0, 2548700.0, 2256480.0] #websearch+incast
    # ax3_data = [2400690.0, 2393660.0, 2381100.0, 2365370.0, 2372680.0, 2062570.0] #fbhdp+incast

    #ICDCS 2024.12.14
    # ax_data = [2447340.0, 2442110.0, 2440470.0, 2438540.0, 2435870.0, 1830330.0] #websearch
    # ax1_data = [2239210.0, 2236980.0, 2232570.0, 2228590.0, 2224050.0, 1676630.0] #fbhdp
    # ax2_data = [2614250.0, 2605850.0, 2599320.0, 2581360.0, 2592700.0, 2004880.0] #websearch+incast
    # ax3_data = [2410100.0, 2406720.0, 2391990.0, 2374570.0, 2389680.0, 1853340.0] #fbhdp+incast

    #ICNP 2025.5.13
    # ax_data = [2466370.0, 2464240.0, 2464480.0, 2455110.0, 2456700.0, 1920750.0] #websearch
    # ax1_data = [2447360.0, 2442120.0, 2441180.0, 2438540.0, 2435880.0, 1899300.0] #fbhdp
    # ax2_data = [2644310.0, 2633540.0, 2628490.0, 2613030.0, 2598960.0, 2161580.0] #websearch+incast
    # ax3_data = [2614280.0, 2605900.0, 2598500.0, 2581400.0, 2592720.0, 2038510.0] #fbhdp+incast

    #INFOCOM 2025.8.2
    # ax_data = [2322300.0, 2320200.0, 2316530.0, 2311890.0, 2301690.0, 1659810.0] #websearch
    # ax1_data = [2234880.0, 2233020.0, 2230100.0, 2228500.0, 2226640.0, 1649920.0] #fbhdp
    # ax2_data = [2490440.0, 2484160.0, 2469450.0, 2456590.0, 2450430.0, 1748540.0] #websearch+incast
    # ax3_data = [2396180.0, 2389360.0, 2379660.0, 2365460.0, 2387500.0, 1847130.0] #fbhdp+incast
    

    # sj 0708号新跑的结果：
    ax_data = [2337350.0, 2332860.0, 2327450.0, 2322230.0, 2315420.0, 2268030.0] #websearch
    ax1_data = [2251370.0, 2244550.0, 2240960.0, 2237900.0, 2237330.0, 2182850.0] #fbhdp
    ax2_data = [2512110.0, 2504080.0, 2492040.0, 2476110.0, 2471000.0, 2440990.0] #websearch+incast
    ax3_data = [2422790.0, 2409660.0, 2397450.0, 2383540.0, 2378740.0, 2315910.0] #fbhdp+incast
    
    '''
    websearch:  [2466370.0, 2464240.0, 2464480.0, 2455110.0, 2456700.0, 1920750.0] 1
    fbhdp:   [2447360.0, 2442120.0, 2441180.0, 2438540.0, 2435880.0, 1899300.0] 1 
    websearch+incast:[2644310.0, 2633540.0, 2628490.0, 2613030.0, 2598960.0, 2161580.0] 1
    fbhdp+incase:   [2614280.0, 2605900.0, 2598500.0, 2581400.0, 2592720.0, 2038510.0] 1
    '''

    ax_data = [item / 1e6 for item in ax_data]
    ax1_data = [item / 1e6 for item in ax1_data]
    ax2_data = [item / 1e6 for item in ax2_data]
    ax3_data = [item / 1e6 for item in ax3_data]

    ax_decreases = calculate_decrease(ax_data)
    ax1_decreases = calculate_decrease(ax1_data)
    ax2_decreases = calculate_decrease(ax2_data)
    ax3_decreases = calculate_decrease(ax3_data)

    print("ax 减少量:", ax_decreases)
    print("ax1 减少量:", ax1_decreases)
    print("ax2 减少量:", ax2_decreases)
    print("ax3 减少量:", ax3_decreases)
    '''
    websearched -n 30 -t 0.02 -l 0.8 -b 100G   
        new [2587440.0, 2578450.0, 2573780.0, 2560670.0, 2548700.0, 2256480.0]
    -c FbHdp_distribution.txt -n 30 -l 0.8 -t 0.02 -b 100G  
        new [2400690.0, 2393660.0, 2381100.0, 2365370.0, 2372680.0, 2062570.0]
        [2400690.0, 2393660.0, 2381100.0, 2365370.0, 2372680.0, 1924640.0]
        100G固定带宽，0端口Incast，1~5端口出，6~29端口入 
        加入Incast流 300Gbps 持续0.5ms  间隔1.0ms Incast流走单独一个端口0
 

    websearched -n 30 -t 0.02 -l 0.8 -b 100G 
                       new [2410080.0, 2411210.0, 2400590.0, 2395250.0, 2403300.0, 2110080.0]
    -c FbHdp_distribution.txt -n 30 -l 0.8 -t 0.02 -b 100G
        new  [2221780.0, 2220170.0, 2218010.0, 2214740.0, 2208180.0, 1895990.0]
        100G固定带宽，1~5端口出，6~29端口入； 正常的websearch，没有incast

    '''
    d02=[ax_data[0],ax1_data[0],ax2_data[0],ax3_data[0]]
    d05=[ax_data[1],ax1_data[1],ax2_data[1],ax3_data[1]]
    d1=[ax_data[2],ax1_data[2],ax2_data[2],ax3_data[2]]
    d2=[ax_data[3],ax1_data[3],ax2_data[3],ax3_data[3]]
    d4=[ax_data[4],ax1_data[4],ax2_data[4],ax3_data[4]]
    fbm=[ax_data[5],ax1_data[5],ax2_data[5],ax3_data[5]]

    ##############################这里，数据都有了
    fig=plt.figure(dpi=600,figsize=(12, 6))
    ax=fig.add_subplot(111)

    # bar_width = 0.3
    # ax.set_ylabel('Loss Packets',fontsize=18);####################################################
    # lns1=ax.bar(x=np.arange(len(x_data))-bar_width/2, width=bar_width, height=ax_data, label='Web Searched', fc = 'steelblue',alpha=0.8)
    # lns2=ax.bar(x=np.arange(len(x_data))+bar_width/2, width=bar_width, height=ax1_data,label='FbHdp',fc = 'indianred',alpha=0.8)
    # plt.xticks(np.arange(len(x_data)), x_data)#np.arange(len(x_data))+bar_width/2
    # ax.set_xlabel('Different Algorithms (D: DeepHir)',fontsize=18)
    # fig.legend(loc=3, borderaxespad=0,bbox_to_anchor=(0.37, 0.92),bbox_transform=ax.transAxes,fontsize='large',frameon=False,ncol=2)

    plt.tick_params(axis='x', labelsize=final_font_size)  # 设置x轴的字体大小
    plt.tick_params(axis='y', labelsize=final_font_size)  # 设置y轴的字体大小


    labels=['WebSearch','Hadoop','WebSearch\n+Incast','Hadoop\n+Incast']
    x = np.arange(len(labels))  # x轴刻度标签位置
    width = 0.12  # 柱子的宽度
    colors_BMS = ['green','blue','purple','k','orange','r']
    # 计算每个柱子在x轴上的位置，保证x轴刻度标签居中
    plt.bar(x - 2.5*width, d02, width, label='D-0.2M', color='green', alpha=0.7, hatch='/', edgecolor='white')
    plt.bar(x - 1.5*width, d05, width, label='D-0.5M', color='blue', alpha=0.7, hatch='\\', edgecolor='white')
    plt.bar(x - 0.5*width, d1, width, label='D-1.0M', color='purple', alpha=0.7, hatch='x', edgecolor='white')
    plt.bar(x + 0.5*width, d2, width, label='D-2.0M', color='k', alpha=0.7, hatch='o', edgecolor='white')
    plt.bar(x + 1.5*width, d4, width, label='D-4.0M', color='orange', alpha=0.7, hatch='+', edgecolor='white')
    plt.bar(x + 2.5*width, fbm, width, label='FBM', color='r', alpha=0.7, hatch='*', edgecolor='white')
    plt.ylabel('# of Packet Loss(x1e6)',fontsize=final_font_size)#Number of Packet Loss
    #plt.xlabel('Different Data for Traffic Generation',fontsize=final_font_size)
    # plt.title('Loss of Different Algorithms of Realistic Traffic (D: DeephHir)',fontsize=18)
    # plt.legend()
    # x轴刻度标签位置不进行计算

    plt.xticks(x, labels=labels)
    # plt.legend()
    plt.legend(bbox_to_anchor=(-0.08, 1.001), loc=3, borderaxespad=0,labels=["DeepHir-0.2M","DeepHir-0.5M","DeepHir-1M","DeepHir-2M","DeepHir-4M","FBM"],ncol=3,fontsize=final_font_size,columnspacing=1.3)
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.ylim([1.0,2.55])
    ax.yaxis.set_major_locator(MultipleLocator(0.5))
    plt.subplots_adjust(bottom=0.15,left=0.15,top=0.8)
    # plt.autoscale()

    # plt.yscale('log')
    output_file = os.path.join(SCRIPT_DIR, "true_flow.pdf")

    plt.savefig(
        output_file,
        bbox_inches='tight'
    )

    print("图片已保存到：", output_file)
    
test9_plot()