#coding:utf-8
import os
import numpy as np
import math
import csv
import re
import sys
import random
import bisect
import traceback
import matplotlib.pyplot as plt
from matplotlib.pyplot import MultipleLocator 
from mpl_toolkits.axes_grid1 import host_subplot
from mpl_toolkits import axisartist


# plt.rcParams['font.sans-serif'] = ['SimSun'] # 用来正常显示中文标签SimHei
plt.rcParams['axes.unicode_minus'] = False # 用来正常显示负号
plt.rc('font',family='Times New Roman')
# plt.rcParams['legend.fontsize'] = 15

# 当前脚本所在目录：tests/analysis
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# tests目录
TESTS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))

# tests/data目录
DATA_ROOT = os.path.join(TESTS_DIR, "data")

# BMS和pbs数据目录
data_dir_BMS = os.path.join(DATA_ROOT, "BMS") + os.sep
data_dir_pbs = os.path.join(DATA_ROOT, "pbs") + os.sep

# 原代码中的data_dir默认指向BMS目录
data_dir = data_dir_BMS

# 图片输出目录
save_path = os.path.join(TESTS_DIR, "data-fig") + os.sep



nPort = 2
nPrio = 2
nQueue = 5

interval = 100
KB = 1024
MB = KB*1024
GB = MB*1024
Gbps = 1000*1000*1000
ns = 1000*1000*1000

Value=1000
font_size=20
x_valaue=3
xy_valaue=20;
linewidth=1

linewidth_middle = 2.0

x_major_locator=MultipleLocator(0.1)
y_major_locator=MultipleLocator(500)

y_major_locator_usage=MultipleLocator(1)
y_major_locator_throughput=MultipleLocator(500)
x_start = 0
x_end = 0.8

nPort = 64
nPrio = 2
nQueue = 5

interval = 100
KB = 1024
MB = KB*1024
GB = MB*1024
Gbps = 1000*1000*1000
ns = 1000*1000*1000
Value=1000

xy_valaue=20
x_valaue=1
linewidth=1

xy_label_fontsize = 20

x_major_locator=MultipleLocator(1)
y_major_locator=MultipleLocator(500)
# x_major_locator=MultipleLocator(0.005)

x_start = 0.0
x_end = 0.8


final_font_size = 25
final_fig_height = 8
final_fig_width = 12


# hw_queue_hbm_usage_test_tc2_01 =f'D:/figure/deephir/queue-hbm-usage-test-tc2-01.csv'
def queue_usage_plot(id,name,flow_rate):
    
    filename = data_dir+'/'+id+'/'+name +'/'+flow_rate+ '/queue-hbm-usage-test-' + id + '.csv'
    hbm_usage = {}


    storing_decision_sram=[]
    time_sram1=[]
    storing_decision_dram=[]
    time_dram1=[]
    storing_decision1_sram=[]
    time_sram2=[]
    storing_decision1_dram=[]
    time_dram2=[]


    #绘制sram
    filename = data_dir+'/'+id+'/'+name +'/'+flow_rate+ '/queue-sram-usage-test-' + id + '.csv'
    sram_usage = {}
    with open(filename) as file:
        lines = file.readlines()
        if len(lines) <= 1:
            return

        lineNum = 0
        inter = interval
        queueNum = 0

        for line in lines[1:]:
            lineNum += 1
            #if lineNum % inter == 0 or lineNum // interval < 2:
            if len(line) < 5:
                continue
            line = line.split(',')
            time0 = (float(line[0])*Value)
            sram0 = (float(line[4])/MB)
            port = line[1]
            prio = line[2]
            queue = line[3]

            if (port, prio, queue) not in sram_usage.keys():
                time = []
                sram = []
                sram_usage[(port, prio, queue)] = (time, sram)
                queueNum += 1
                inter = round(interval / queueNum)
            else:
                sram_usage[(port, prio, queue)][0].append(time0)
                sram_usage[(port, prio, queue)][1].append(sram0)
                    
                        
    if len(sram_usage) == 0:
        return
    
    for key in sram_usage.keys():
        port = key[0]
        prio = key[1]
        queue = key[2]
        if str(port)=="0":
            plt.plot(sram_usage[key][0], sram_usage[key][1], label='port0-S',linewidth=linewidth,linestyle='--',color='k')

        if str(port)=="1":
            plt.plot(sram_usage[key][0], sram_usage[key][1], label='port1-S',linewidth=linewidth,linestyle='-',color='red')
        if str(port)=="2":
            plt.plot(sram_usage[key][0], sram_usage[key][1], label='port2-S',linewidth=linewidth,linestyle=':',color='green')

    #绘制队列占用
    file_pre = 'hybrid-buffer-test-'+id #文件前缀
    file_name = data_dir +'/'+id+'/'+name +'/'+flow_rate+'/'+ file_pre + '.txt' #....../hybPrid-buffer-test-tc2-01.txt
    with open(file_name) as file:
        time_0 = []
        queueused_0 = []
        time_1 = []
        queueused_1 = []
        time_2 = []
        queueused_2 = []
        for line in file:
            if "xzx_debug_sign" not in line:
                #print("skip this line")
                continue
            data = line.strip().split()
            #data:110912148 10912148ns
            #['100001866', 'middle_value_for_plot:', 'port:', '0', "CurrentCycle(T'th):", '1', 'newT[T+1](ns):', '8388', 
            # 'newU[T+1]', 'Usram:', '1.333', 'Udram:', '1.18475', 'lambda:', '898.72', 'miu(Gbps):', '778.56', 'Sr(MB):', 
            # '4.1943', 'Dr(Gbps):', '1000', '21 U_star[T]:', '0.999999', '23 U[T]:', '0', 'Storing_decision(0片外-1片内-2丢包):', 
            # '1', 'final_dicision', '1']
            
            #port0
            if int(data[3]) == 0:
                time_0.append((float(data[0]))/1000000.0) #ns->ms
                queueused_0.append(float(data[5]))
            if int(data[3]) == 1:
                time_1.append((float(data[0]))/1000000.0) #ns->ms
                queueused_1.append(float(data[5]))
            if int(data[3]) == 2:
                time_2.append((float(data[0]))/1000000.0) #ns->ms
                queueused_2.append(float(data[5]))
        plt.plot(time_0, queueused_0, label='port0-queue',linewidth=linewidth,linestyle='-',color='blue')
        plt.plot(time_1, queueused_1, label='port1-queue',linewidth=linewidth,linestyle='-',color='orange')        
        plt.plot(time_2, queueused_2, label='port2-queue',linewidth=linewidth,linestyle='-',color='purple')
    plt.tight_layout()
    plt.legend().set_visible(True)
    #取出句柄和标签

    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    path = data_dir+'/'+id+'/'+name +'/'+flow_rate+'/' +'queue-usage-' + id +  '.pdf'
    plt.savefig(path)
    # plt.show()
    plt.clf()

def buffer_loss_compare(id):
    output_dir = os.path.join(
        save_path,
        "compare",
        id
    )
    os.makedirs(output_dir, exist_ok=True)
    # pbs filename
    filename = data_dir_pbs + id + '/' + "buffer-usage-test-"+id+".csv"
    # BMS filename
    filename_BMS = data_dir_BMS + id + '/' +"buffer-usage-test-"+id+".csv"
    loss_packet =data_dir_pbs + id +"/loss_packet.csv"
    loss_packet_BMS =data_dir_BMS + id +"/loss_packet.csv"

    

    time_pbs = []
    sram_pbs = []
    dram_pbs = []
    time_loss_pbs = []
    loss_pbs = []

    time_BMS = []
    sram_BMS = []
    dram_BMS = []
    time_loss_BMS = []
    loss_BMS = []

    with open(filename) as file_pbs,open(filename_BMS) as file_BMS:
        for line_pbs in file_pbs:
            line = line_pbs.split(',')
            if len(line) <4:
                continue
            if line[0] == "time":
                continue
            time_pbs.append(float(line[0])*Value)
            sram_pbs.append(float(line[1])/MB)
            dram_pbs.append(float(line[3])/MB)

        lines = file_BMS.readlines()
        for line_BMS in lines[1:]:
            line = line_BMS.split(',')
            if len(line) <4:
                continue
            time_BMS.append(float(line[0])*Value)
            sram_BMS.append(float(line[1])/MB)
            dram_BMS.append(float(line[3])/MB)


    with open(loss_packet) as file_pbs,open(loss_packet_BMS) as file_BMS:
        lines = file_pbs.readlines()
        lines_BMS = file_BMS.readlines()
        if len(lines)<=1 or len(lines_BMS)<=1:
            return
        
        interval=50
        count=0
        for line in lines:
            
            if len(line) < 6:
                continue
            line = line.split(',')
            try:
                cur = float(line[1])
            except ValueError:
                print("出现文字转浮点数失败,自动跳过,详细错误如下：")
                print(traceback.format_exc())
                continue
            count+=1
            if count%interval!=0:
                continue
            loss_pbs.append(float(line[5])/ 1000.0)
            time_loss_pbs.append(cur*Value)


        for line_BMS in lines_BMS:
            if len(line_BMS) < 4:
                continue
            line_BMS = line_BMS.split(',')
            try:
                cur = float(line_BMS[1])
            except ValueError:
                print("出现文字转浮点数失败,自动跳过,详细错误如下：")
                print(traceback.format_exc())
                continue
            loss_BMS.append(float(line_BMS[5])/ 1000.0)
            time_loss_BMS.append(cur*Value)


    plt.xticks(fontsize=final_font_size)
    plt.xlabel('Time(ms)',fontsize=final_font_size)

    # plt.rcParams['figure.figsize']=(12.8, 7.2)
    # 全局设置输出图片大小 1280 x 720 像素
    
    #plt.yscale("log",basey=2)
    plt.yticks(fontsize=final_font_size)
    plt.ylabel('Buffer Usage(MB)',fontsize=final_font_size)
    plt.tick_params(axis='x',direction='in')
    plt.tick_params(axis='y',direction='in')
    ax=plt.gca()
    
    
    if id == 'tc2-06':
        plt.ylim(0,4)
        ax.yaxis.set_major_locator(MultipleLocator(1))
        # ax.xaxis.set_major_locator(MultipleLocator(5))
    if id == 'tc2-07':
        plt.ylim(0,5)
        plt.xlim(0,8)
        ax.xaxis.set_major_locator(MultipleLocator(1))
    


    lines = [0,0]

    
    lines[0], =plt.plot(time_BMS, sram_BMS, label='DeepHir',linewidth=linewidth+2,linestyle='--',color='k')
    lines[1], =plt.plot(time_pbs, sram_pbs, label='FBM',linewidth=linewidth+2,linestyle='-',color='red')
    
    # plt.plot(time_pbs, dram_pbs, label='FBM-DRAM',linewidth=linewidth,linestyle='-',color='green')
    # plt.plot(time_BMS, dram_BMS, label='DeepHir-DRAM',linewidth=linewidth,linestyle='-',color='orange')
    
    handles,labels = ax.get_legend_handles_labels()


    plt.tight_layout()
    plt.legend().set_visible(True)
    plt.subplots_adjust(top=0.85,left=0.19,right=0.86)

    
    plt.legend(bbox_to_anchor=(-0.03, 0.75), loc=3,handles=lines,handletextpad=0.2,labels=["DeepHir","FBM"],ncol=2,columnspacing=0.6,fontsize = final_font_size,frameon=False, facecolor='none')

    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)

    path = os.path.join(
        output_dir,
        "buffer-BMS-vs-pbs.pdf"
    )
    plt.savefig(path, bbox_inches='tight')
    plt.clf()


    plt.xticks(fontsize=final_font_size)
    plt.xlabel('Time(ms)',fontsize=final_font_size)
    plt.ylabel('# of Packet Loss(x1e3)',fontsize=final_font_size)
    plt.yticks(fontsize=final_font_size)
    plt.tick_params(axis='x',direction='in')
    plt.tick_params(axis='y',direction='in')

    ax=plt.gca()

    # if id == 'tc2-05':
    #     ax.xaxis.set_major_locator(MultipleLocator(0.4))
    if id == 'tc2-06':
        ax.xaxis.set_major_locator(MultipleLocator(4))
    if id == 'tc2-07':
        ax.xaxis.set_major_locator(MultipleLocator(5))
    if id == 'tc2-05':
        # plt.ylim(0,4)
        ax.xaxis.set_major_locator(MultipleLocator(0.4))

    plt.plot(time_loss_BMS,loss_BMS,linestyle='--',linewidth = linewidth+2,color='black',label='DeepHir')
    plt.plot(time_loss_pbs,loss_pbs,label='FBM',linestyle='-',linewidth = linewidth+2,color='red')
    
    

    plt.legend(fontsize=final_font_size)
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)

    path = os.path.join(
        output_dir,
        "loss-BMS-vs-pbs.pdf"
    )
    plt.savefig(path, bbox_inches='tight')
    plt.clf()

def get_last_field(file_name):
    with open(file_name, 'rb') as f:
        f.seek(-2, 2)  # 移动到倒数第二个字节
        while f.read(1) != b'\n':  # 读到换行符为止
            f.seek(-2, 1)  # 继续向前移动
        last_line = f.readline().decode()  # 读取最后一行
    return last_line.strip().split(',')[-1]  # 获取最后一项

def test8_plot():
    final_font_size = 26

    # tests/data目录，末尾保留路径分隔符
    data_dir = DATA_ROOT + os.sep

    # 使用全局图片输出目录
    output_root = save_path

    # Deephir
    # 定义文件路径
    flow_rates = [100,200,300,400,500,600,700,800,900]
    thresholds = [0.2,0.5,1.0,2.0,4.0]
    plt.figure()
    markers_BMS = ['o','v','1','s','P']
    colors_BMS = ['green','blue','purple','k','orange']
    file_path_pre = data_dir + 'BMS/tc2-08/'
    i = 0
    lines = [0,0,0,0,0,0]

    # plt.yscale("symlog")
    # plt.figure(figsize=(12, 8))
    plt.xticks(fontsize = 26) 
    plt.yticks(fontsize = 26) 
    for threshold in thresholds:
        y = []
        for flow_rate in flow_rates:
            file_path = file_path_pre + str(threshold) +'M/' + str(flow_rate) +'/loss_packet.csv'
            # 打开文件并读取最后一行的最后一项
            loss_num = float(get_last_field(file_path))
            y.append(loss_num)
        epsilon = 1e-15  # 设置一个极小值
        y[y==0] = epsilon
        # 绘制折线图
        lines[i], =plt.plot(flow_rates, y,marker=markers_BMS[i], linestyle='-', color=colors_BMS[i],linewidth =linewidth_middle+2)
        i = i+1
    # pbs
    file_path_pre = data_dir + 'pbs/tc2-08/'
    y = []
    for flow_rate in flow_rates:
        file_path = file_path_pre + str(flow_rate) +'/loss_packet.csv'
        # 打开文件并读取最后一行的最后一项
        loss_num = float(get_last_field(file_path))
        y.append(loss_num)

    # 绘制折线图
    lines[i], =plt.plot(flow_rates, y, marker='>', linestyle='-', color='r',linewidth =linewidth_middle+4)
    plt.xlabel("Flow Rate(Gbps)",fontsize=final_font_size)
    plt.ylabel("# of Packet Loss",fontsize=final_font_size)

    plt.legend(bbox_to_anchor=(-0.02, 1.001), loc=3, columnspacing=0.8,borderaxespad=0,handles=lines,labels=["Deephir-0.2M","Deephir-0.5M","Deephir-1M","Deephir-2M","Deephir-4M","FBM"],ncol=3,fontsize=final_font_size)
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    output_dir = os.path.join(
        output_root,
        "compare",
        "tc2-08"
    )

    os.makedirs(output_dir, exist_ok=True)

    path = os.path.join(
        output_dir,
        "adaptability-to-Traffic-Variation-compare.pdf"
    )
    plt.savefig(path, bbox_inches='tight')
    plt.clf()

# queue_usage_plot("tc2-08","0.2M","900")
# queue_usage_plot("tc2-08","0.2M","500")
# queue_usage_plot("tc2-08","1.0M","900")
# queue_usage_plot("tc2-08","2.0M","500")
# queue_usage_plot("tc2-08","2.0M","700")
# queue_usage_plot("tc2-08","2.0M","900")
# queue_usage_plot("tc2-08","3.0M","900")
# queue_usage_plot("tc2-08","4.0M","900")
test8_plot()
buffer_loss_compare("tc2-05")
buffer_loss_compare("tc2-06")
buffer_loss_compare("tc2-07")
