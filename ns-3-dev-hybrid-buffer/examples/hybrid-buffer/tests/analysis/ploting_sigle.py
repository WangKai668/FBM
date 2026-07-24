#coding:utf-8
import os
import numpy as np
import math
import csv
import re
import sys
import random
import bisect
import matplotlib.pyplot as plt
from matplotlib.pyplot import MultipleLocator 
from mpl_toolkits.axes_grid1 import host_subplot
from mpl_toolkits import axisartist
from brokenaxes import brokenaxes  
# 当前脚本所在目录，例如 tests/analysis
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# tests 目录
TESTS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))

# tests/data 和 tests/data-fig
DATA_DIR = os.path.join(TESTS_DIR, "data")
FIG_DIR = os.path.join(TESTS_DIR, "data-fig")

# 保留末尾分隔符，兼容后面大量字符串拼接代码
data_dir = DATA_DIR + os.sep
save_path = FIG_DIR + os.sep


# plt.rcParams['font.sans-serif'] = ['SimSun'] # 用来正常显示中文标签SimHei
plt.rcParams['axes.unicode_minus'] = False # 用来正常显示负号
plt.rc('font',family='Times New Roman')
plt.rcParams['legend.fontsize'] = 15



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
linewidth=1

linewidth_middle = 2.0

x_major_locator=MultipleLocator(0.1)
y_major_locator=MultipleLocator(500)

y_major_locator_usage=MultipleLocator(1)
y_major_locator_throughput=MultipleLocator(500)
x_start = 0
x_end = 0.8


final_font_size = 30 # 24 30


# hw_queue_hbm_usage_test_tc2_01 =f'D:/figure/deephir/queue-hbm-usage-test-tc2-01.csv'
def queue_usage_plot(id,name,iftest8SendRate=''):
    
    filename = data_dir+name+'/'+id+'/' +iftest8SendRate+'/'+ '/queue-hbm-usage-test-' + id + '.csv'
    filename2 = data_dir+name+'/'+id+'/'+iftest8SendRate+'/' + '/queue-sram-usage-test-' + id + '.csv'
    filename3= data_dir+name+'/'+id+'/'+iftest8SendRate+'/'+'hybrid-buffer-test-'+id+'.txt'
    hbm_usage = {}
    
    with open(filename) as file,open(filename2) as file2:
        lines = file.readlines()
        lines2 = file2.readlines()
        if len(lines) <= 1:
            return
        if len(lines2) <= 1:
            return
        print ("yes")

        lineNum = 0
        queueNum = 0

        for line in lines[1:]:
            lineNum += 1
            #if lineNum % inter == 0 or lineNum // interval < 2:
            line = line.split(',')
            if len(line) < 5:
                continue
            time0 = (float(line[0])*Value)
            hbm0 = (float(line[4])/MB)
            port = line[1]
            prio = line[2]
            queue = line[3]

            if (port, prio, queue) not in hbm_usage.keys():
                time = []
                hbm = []
                hbm_usage[(port, prio, queue)] = (time, hbm)
                queueNum += 1
            else:
                hbm_usage[(port, prio, queue)][0].append(time0)
                hbm_usage[(port, prio, queue)][1].append(hbm0)

    storing_decision_sram=[]
    time_sram1=[]
    storing_decision_dram=[]
    time_dram1=[]
    storing_decision1_sram=[]
    time_sram2=[]
    storing_decision1_dram=[]
    time_dram2=[]

    if name=='pbs':
        with open(filename3) as file:
            for line in file:
                if "middle_value_for_plot" not in line:
                    continue
                data = line.strip().split()
            
                #port0
                if int(data[3]) == 0:
                    
                    if int(data[26])==1:
                        storing_decision_sram.append(1.5)
                        time_sram1.append((float(data[0]))/1000000.0)
                    else:
                        storing_decision_dram.append(2)
                        time_dram1.append((float(data[0]))/1000000.0)
                elif int(data[3]) == 1:
                    #port1
                    if int(data[26])==1:
                        storing_decision1_sram.append(3)
                        time_sram2.append((float(data[0]))/1000000.0)
                    else:
                        storing_decision1_dram.append(3.5)
                        time_dram2.append((float(data[0]))/1000000.0)
            
    if len(hbm_usage) == 0:
        return

    plt.figure(figsize=(8, 7.2), dpi=300)

    if id == "tc2-06":
        plt.xlim(x_start,x_end)
        plt.xticks(fontsize=font_size*0.5)

    plt.xticks(fontsize=final_font_size)
    plt.ylim(-0.2,4)

    plt.xlabel('Time (ms)',fontsize=final_font_size)
    plt.ylabel('Buffer Usage (MB)',fontsize=final_font_size)
    plt.yticks(fontsize=final_font_size)  # 设置纵坐标刻度字体大小为10
    plt.tick_params(axis='x', direction='in')
    plt.tick_params(axis='y', direction='in')
    ax=plt.gca()
    ax.yaxis.set_major_locator(MultipleLocator(1))
    ax.xaxis.set_major_locator(MultipleLocator(0.4))

    line_rate=3

    for key in hbm_usage.keys():
        port = key[0]
        if str(port)=="0":
            if name=='pbs':
                plt.plot(hbm_usage[key][0], hbm_usage[key][1], label='p1-D',linewidth=line_rate*linewidth,linestyle='-.',color='r')
            else:
                plt.plot(hbm_usage[key][0], hbm_usage[key][1], label='port1-DRAM',linewidth=line_rate*linewidth,linestyle='-.',color='r')
        if str(port)=="1":
            if name=='pbs':
                plt.plot(hbm_usage[key][0], hbm_usage[key][1], label='p2-D',linewidth=line_rate*linewidth,linestyle=':',color='red')
            else:
                plt.plot(hbm_usage[key][0], hbm_usage[key][1], label='port2-DRAM',linewidth=line_rate*linewidth,linestyle=':',color='red')

    #绘制sram
    filename = data_dir+name+'/'+id+'/' +iftest8SendRate+'/'+ '/queue-sram-usage-test-' + id + '.csv'
    sram_usage = {}
    with open(filename) as file:
        lines = file.readlines()
        if len(lines) <= 1:
            return

        lineNum = 0
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
            if name=='pbs':
                plt.plot(sram_usage[key][0], sram_usage[key][1], label='p1-S',linewidth=line_rate*linewidth,linestyle='--',color='g')
            else:
                plt.plot(sram_usage[key][0], sram_usage[key][1], label='port1-SRAM',linewidth=line_rate*linewidth,linestyle='--',color='g')
        if str(port)=="1":
            if name=='pbs':
                plt.plot(sram_usage[key][0], sram_usage[key][1], label='p2-S',linewidth=line_rate*linewidth,linestyle='-',color='g')
            else:
                plt.plot(sram_usage[key][0], sram_usage[key][1], label='port2-SRAM',linewidth=line_rate*linewidth,linestyle='-',color='g')

    if name=='pbs':
        plt.scatter(time_sram1,storing_decision_sram,label='p1-S ',color='g',s=50,marker='o')
        plt.scatter(time_dram1,storing_decision_dram,label='p1-D ',color='r',s=50,marker='o')
        plt.scatter(time_sram2,storing_decision1_sram,label='p2-S ',color='g',s=50, facecolors='none')
        plt.scatter(time_dram2,storing_decision1_dram,label='p2-D ',color='r',s=50, facecolors='none')

    # plt.tight_layout()
    plt.legend().set_visible(True)
    #取出句柄和标签
    handles, labels = plt.gca().get_legend_handles_labels()
    if name=='pbs':
        if 'p2-D' not in labels:
            time_D=[]
            ans=[]
            last_time=0
            interval=1e-2
            while(last_time<2):
                time_D.append(last_time)
                ans.append(0)
                last_time+=interval
            plt.plot(time_D, ans, label='p2-D',linewidth=line_rate*linewidth,linestyle=':',color='red')
    else:
        if 'port2-DRAM' not in labels:
            time_D=[]
            ans=[]
            last_time=0
            interval=1e-2
            while(last_time<2):
                time_D.append(last_time)
                ans.append(0)
                last_time+=interval
            plt.plot(time_D, ans, label='port2-DRAM',linewidth=line_rate*linewidth,linestyle=':',color='red')

    handles, labels = plt.gca().get_legend_handles_labels()
    #调整标签的顺序
    if name=='pbs':
        if len(labels) == 8:
            str0 = ['p1-S','p2-S','p1-D','p2-D','p1-S ','p2-S ','p1-D ','p2-D ']
            for i in range(0,8):
                j = labels.index(str0[i])
                labels[j],labels[i] = labels[i],labels[j]
                handles[j],handles[i] = handles[i],handles[j]
                plt.legend(handles = handles[:8],labels = labels[:8],loc=(-0.05, 0.97),ncol=4,fontsize=final_font_size,handlelength=1.1, columnspacing=0.35,handletextpad=0.2, frameon=False)

    elif name=='BMS':
        if len(labels) == 4:
            str0 = ['port1-SRAM','port2-SRAM','port1-DRAM','port2-DRAM']
            for i in range(0,4):
                j = labels.index(str0[i])
                labels[j],labels[i] = labels[i],labels[j]
                handles[j],handles[i] = handles[i],handles[j]
            plt.legend(
                handles=handles[:4],
                labels=labels[:4],
                loc='lower center',
                bbox_to_anchor=(0.5, 1.02),
                ncol=2,
                fontsize=20,
                handlelength=1.8,
                columnspacing=1.0,
                handletextpad=0.5,
                frameon=False
            )
    plt.subplots_adjust(
        left=0.16,
        right=0.98,
        bottom=0.16,
        top=0.72
    )
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    path = save_path+name+'/'+id+'/'+iftest8SendRate+'/' +'queue-usage-' + id +  '.pdf'
    plt.savefig(
        path,
        bbox_inches='tight',
        pad_inches=0.05
    )
    # plt.show()
    plt.clf()

#片内片外缓存吞吐率
def queue_write_read_throughput_plot(test_case_number,name,iftest8SendRate=""):
    #---------------------绘制片内------------------#
    file_pre1 = 'queue-sram-write-throughput-test-'+test_case_number #片内写文件前缀
    file_pre2 = 'queue-sram-read-throughput-test-'+test_case_number #片内读文件前缀
    save_png_name = 'sram-throughput-withport-test-'+test_case_number
    num_M=''
    case_name=''
    print('***prt224***:马上是if')#','+test_case_number+'.contains(\'tc2-09/\')='+str(test_case_number).__contains__('tc2-09/'))
    if str(test_case_number).__contains__('tc2-09/') :#####################################################################################################
        num_M=test_case_number.split('/')[1]
        case_name=test_case_number.split('/')[0]
        file_pre1 = 'queue-sram-write-throughput-test-'+case_name #片内写文件前缀
        file_pre2 = 'queue-sram-read-throughput-test-'+case_name #片内读文件前缀
        save_png_name = 'sram-throughput-withport-test-'+case_name
    # Port/Queue throughput
    #写端口
    write_thput = {}
    for port in range(nPort):
        # hw
        filename = data_dir+name+'/'+iftest8SendRate+'/'+test_case_number+'/' +file_pre1 + "-p"+ str(port) + ".csv"

        print(f"prt*238*:filename={filename}")
        # filename = f'D:/figure/pbs/'+file_pre1 + "-p"+ str(port) + ".csv"

        with open(filename) as file:
            lines = file.readlines()
            if len(lines) <= 1:
                continue
            line1 = lines[1].split(',')
            line_1 = lines[-1].split(',')

            start0 = float(line1[0])*ns*Value
            end0 = float(line_1[1])*ns*Value
            inter = float(line1[1])*ns*Value-start0
            time = [t/ns for t in np.arange(start0, end0+inter, inter)]

            for line in lines[1:]:
                line = line.split(',')
                if len(line) < 5:
                    continue
                start = float(line[0])*ns*Value
                end = float(line[1])*ns*Value
            
                prio = line[2]
                queue = line[3]
                write_rate = float(line[4][:-1])/Gbps

                if (port, prio, queue) not in write_thput.keys():
                    time_t = []
                    thput_t = [0]*len(time)
                    write_thput[(port, prio, queue)] = (time, thput_t)
                else:
                    write_thput[(port, prio, queue)][1][round((int(end)-start0)/inter)] = write_rate

    # write_throughput_plot
    if len(write_thput) == 0:
        return


    #读端口
    read_thput = {}
    for port in range(nPort):
        # hw
        filename = data_dir+name+'/'+iftest8SendRate+'/'+test_case_number+'/' + file_pre2 + "-p" + str(port) + ".csv"
        # filename = f'D:/figure/pbs/'+file_pre2 + "-p"+ str(port) + ".csv"

        with open(filename) as file:
            lines = file.readlines()
            if len(lines) <= 1:
                continue

            line1 = lines[1].split(',')
            line_1 = lines[-1].split(',')

            start0 = float(line1[0])*ns*Value
            end0 = float(line_1[1])*ns*Value
            inter = float(line1[1])*ns*Value-start0
            time = [t/ns for t in np.arange(start0, end0+inter, inter)]
            for line in lines[1:]:
                line = line.split(',')
                if len(line) < 5:
                    continue
                start = float(line[0])*ns*Value
                end = float(line[1])*ns*Value
            
                prio = line[2]
                queue = line[3]
                read_rate = float(line[4])/Gbps

                if (port, prio, queue) not in read_thput.keys():
                    time_t = []
                    thput_t = [0]*len(time)
                    read_thput[(port, prio, queue)] = (time, thput_t)
                else:
                    read_thput[(port, prio, queue)][1][round((int(end)-start0)/inter)] = read_rate

    # read_throughput_plot
    if len(read_thput) == 0:
        return

    if test_case_number == "tc2-01":
        # plt.xlim(x_start,x_end*1.25)
        plt.xticks(fontsize=font_size)  # 设置横坐标刻度字体大小为10
    elif test_case_number == "tc2-02":
        # plt.xlim(x_start,x_end*0.75)
        plt.xticks(fontsize=font_size)
    elif test_case_number == "tc2-03":
        # plt.xlim(x_start,x_end*1.75)
        plt.xticks(fontsize=font_size)
    elif test_case_number == "tc2-06":
        plt.xlim(x_start,x_end)
        plt.xticks(fontsize=font_size*0.5)
    else :
        # plt.xlim(x_start,x_end*2.5)
        plt.xticks(fontsize=font_size)

    plt.ylim(-40,1600)

    plt.xlabel('Times(ms)',fontsize=font_size*0.8)
    plt.ylabel('SRAM_throughput(Gbps)',fontsize=font_size*0.8)

    plt.yticks(fontsize=font_size)  # 设置纵坐标刻度字体大小为10
    plt.tick_params(axis='x', direction='in')
    plt.tick_params(axis='y', direction='in')
    ax=plt.gca()
    #ax.xaxis.set_major_locator(MultipleLocator(0.2))
    ax.yaxis.set_major_locator(MultipleLocator(200))


    for key in read_thput.keys():
        port = key[0]
        prio = key[1]
        queue = key[2]
        if str(port)=="0":
            time_float = [float(value) for value in read_thput[key][0]]
            #time_float_adjusted = [x - 100 for x in time_float]
            plt.plot(time_float, read_thput[key][1], label='port1_read',linewidth=linewidth,marker='*',linestyle='--',color='dimgray',markersize=1)

        if str(port)=="1":
            time_float = [float(value) for value in read_thput[key][0]]
            #time_float_adjusted = [x - 100 for x in time_float]
            plt.plot(time_float, read_thput[key][1], label='port2_read',linewidth=linewidth,marker='o',linestyle='--',color='lightcoral',markersize=1)

        
    for key in write_thput.keys():

        port = key[0]
        prio = key[1]
        queue = key[2]
        if str(port)=="0":
            time_float = [float(value) for value in write_thput[key][0]]
            #time_float_adjusted = [x - 100 for x in time_float]
            plt.plot(time_float, write_thput[key][1], label='port1_write',linewidth=linewidth,marker='^',linestyle='-',color='k',markersize=1)

            # tt=[0.406,0.407,0.408]
            # wt=[0,688.192,234.064]
            # plt.plot(tt, wt, label='写端口2',linewidth=linewidth)


        if str(port)=="1":
            time_float = [float(value) for value in write_thput[key][0]]
            #time_float_adjusted = [x - 100 for x in time_float]

            # tt=[0.406,0.407,0.408]
            # wt=[0,688.192,234.064]
            # plt.plot(tt, wt, label='写端口2',linewidth=linewidth,marker='s',linestyle='-',color='orange')
            
            plt.plot(time_float, write_thput[key][1], label='port2_write',linewidth=linewidth,marker='s',linestyle='-',color='r',markersize=1)


        
    plt.tight_layout()
    plt.legend().set_visible(True)
    plt.legend(loc="upper right")
    #plt.legend(loc='upper left')
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    path = save_path+name+'/'+iftest8SendRate+'/'+test_case_number+'/'+ save_png_name + '.png'
    plt.savefig(path)
    # plt.show()
    plt.clf()
    #---------------------绘制片内结束------------------#

    #---------------------绘制片外------------------#
    file_pre1 = 'queue-hbm-write-throughput-test-'+test_case_number #片外写文件前缀
    file_pre2 = 'queue-hbm-read-throughput-test-'+test_case_number #片外读文件前缀
    save_png_name = 'hbm-throughput-withport-test-'+test_case_number
    # Port/Queue throughput
    #写端口
    write_thput = {}
    for port in range(3):
        # hw
        filename = data_dir+name+'/'+iftest8SendRate+'/'+test_case_number+'/' +file_pre1 + "-p"+ str(port) + ".csv"
        # filename = f'D:/figure/pbs/'+file_pre1 + "-p"+ str(port) + ".csv"

        with open(filename) as file:
            lines = file.readlines()
            if len(lines) <= 1:
                continue
            line1 = lines[1].split(',')
            line_1 = lines[-1].split(',')

            start0 = float(line1[0])*ns*Value
            end0 = float(line_1[1])*ns*Value
            inter = float(line1[1])*ns*Value-start0
            time = [t/ns for t in np.arange(start0, end0+inter, inter)]

            for line in lines[1:]:
                line = line.split(',')
                if len(line) < 5:
                    continue
                start = float(line[0])*ns*Value
                end = float(line[1])*ns*Value
            
                prio = line[2]
                queue = line[3]
                write_rate = float(line[4][:-1])/Gbps

                if (port, prio, queue) not in write_thput.keys():
                    time_t = []
                    thput_t = [0]*len(time)
                    write_thput[(port, prio, queue)] = (time, thput_t)
                else:
                    write_thput[(port, prio, queue)][1][round((int(end)-start0)/inter)] = write_rate

    # write_throughput_plot
    if len(write_thput) == 0:
        return

    #读端口
    read_thput = {}
    for port in range(3):
        # hw
        filename = data_dir+name+'/'+iftest8SendRate+'/'+test_case_number+'/' + file_pre2 + "-p" + str(port) + ".csv"
        # filename = f'D:/figure/pbs/'+file_pre2 + "-p"+ str(port) + ".csv"

        with open(filename) as file:
            lines = file.readlines()
            if len(lines) <= 1:
                continue

            line1 = lines[1].split(',')
            line_1 = lines[-1].split(',')

            start0 = float(line1[0])*ns*Value
            end0 = float(line_1[1])*ns*Value
            inter = float(line1[1])*ns*Value-start0
            time = [t/ns for t in np.arange(start0, end0+inter, inter)]
            for line in lines[1:]:
                line = line.split(',')
                if len(line) < 5:
                    continue
                start = float(line[0])*ns*Value
                end = float(line[1])*ns*Value
            
                prio = line[2]
                queue = line[3]
                read_rate = float(line[4])/Gbps

                if (port, prio, queue) not in read_thput.keys():
                    time_t = []
                    thput_t = [0]*len(time)
                    read_thput[(port, prio, queue)] = (time, thput_t)
                else:
                    read_thput[(port, prio, queue)][1][round((int(end)-start0)/inter)] = read_rate

    # read_throughput_plot
    if len(read_thput) == 0:
        return

    # time=[]
    # sum=[]


    #print(sum)
    # plt.figure(figsize=(9,7))
    # plt.subplots_adjust(left=0.14, right=0.98,bottom=0.15)
    plt.figure(figsize=(8, 7.2), dpi=300)
    plt.xticks(fontsize=final_font_size)
    plt.xlabel('Time (ms)',fontsize=final_font_size)
    plt.ylabel('Throughput (Gbps)',fontsize=final_font_size)

    plt.yticks(fontsize=final_font_size)  # 设置纵坐标刻度字体大小为10
    plt.tick_params(axis='x', direction='in')
    plt.tick_params(axis='y', direction='in')
    ax=plt.gca()
    if test_case_number == "tc2-06":
        plt.xlim(-0.05,0.8)
        ax.xaxis.set_major_locator(MultipleLocator(0.1))

    elif test_case_number == "tc2-05":
        plt.xlim(-0.05,2.1)
        ax.xaxis.set_major_locator(MultipleLocator(0.4))


    if test_case_number == "tc2-06":
        plt.ylim(-20,1000)
    elif name=='pbs' and test_case_number == "tc2-05":
        plt.ylim(-20,400)
    else:
        plt.ylim(-40,800)
    ax.yaxis.set_major_locator(MultipleLocator(200))


    # plt.plot(time_hw, read_hw, label='读片外',linewidth=5,color='red')
    # plt.plot(time_hw, write_hw, label='写片外',linewidth=5,color='green',linestyle='--')
    # plt.plot(time1, loss, label='丢包',linewidth=5,color='blue',linestyle='-.')

    line_rate=2
    markersize=5
    for key in write_thput.keys():

        port = key[0]
        prio = key[1]
        queue = key[2]
        if str(port)=="0":
            time_float = [float(value) for value in write_thput[key][0]]
            #time_float_adjusted = [x - 100 for x in time_float]
            plt.plot(time_float, write_thput[key][1], label='port1_write',linewidth=line_rate*linewidth,marker='^',linestyle='-',color='r',markersize=markersize)
            # tt=[0.406,0.407,0.408]
            # wt=[0,688.192,234.064]
            # plt.plot(tt, wt, label='写端口2',linewidth=linewidth)


        if str(port)=="1":
            time_float = [float(value) for value in write_thput[key][0]]
            #time_float_adjusted = [x - 100 for x in time_float]
            # tt=[0.406,0.407,0.408]
            # wt=[0,688.192,234.064]
            # plt.plot(tt, wt, label='写端口2',linewidth=linewidth,marker='s',linestyle='-',color='orange')
            plt.plot(time_float, write_thput[key][1], label='port2_write',linewidth=line_rate*linewidth,marker='s',linestyle='-',color='g',markersize=markersize)
    

    for key in read_thput.keys():
        port = key[0]
        prio = key[1]
        queue = key[2]
        if str(port)=="0":
            time_float = [float(value) for value in read_thput[key][0]]
            #time_float_adjusted = [x - 100 for x in time_float]
            plt.plot(time_float, read_thput[key][1], label='port1_read',linewidth=line_rate*linewidth,marker='*',linestyle='--',color='orange',markersize=markersize)

        if str(port)=="1":
            time_float = [float(value) for value in read_thput[key][0]]
            #time_float_adjusted = [x - 100 for x in time_float]
            plt.plot(time_float, read_thput[key][1], label='port2_read',linewidth=line_rate*linewidth,marker='o',linestyle='--',color='b',markersize=markersize)
    
    #获取句柄和标签
    handles,labels = plt.gca().get_legend_handles_labels()
    #加入port2_write,port2_read的标签
    if 'port2_write' not in labels:
        interval=1e-3
        time_port2=[]
        write =[]
        last_time=0
        while(last_time+interval<2):
            time_port2.append(last_time)
            write.append(0)
            last_time+=interval
        plt.plot(time_port2,write, label='port2_write',linewidth=linewidth,marker='s',linestyle='-',color='r',markersize=1)
    if 'port2_read' not in labels:
        interval=1e-3
        time_port2=[]
        read =[]
        last_time=0
        while(last_time+interval<2):
            time_port2.append(last_time)
            read.append(0)
            last_time+=interval
        plt.plot(time_port2,read, label='port2_read',linewidth=linewidth,marker='o',linestyle='--',color='lightcoral',markersize=1)
    handles,labels = plt.gca().get_legend_handles_labels()
    if len(labels) == 4:
        str0 = ['port1_write','port2_write','port1_read','port2_read']
        for i in range(0,4):
            j = labels.index(str0[i])
            labels[j],labels[i] = labels[i],labels[j]
            handles[j],handles[i] = handles[i],handles[j]

    
    plt.legend(
        loc='lower center',
        bbox_to_anchor=(0.5, 1.02),
        ncol=2,
        fontsize=20,
        frameon=False,
        columnspacing=1.0,
        handlelength=1.8,
        handletextpad=0.5
    )

    plt.subplots_adjust(
        left=0.16,
        right=0.98,
        bottom=0.16,
        top=0.72
    )
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    path = save_path+name+'/'+iftest8SendRate+'/'+test_case_number+'/' + save_png_name + '.pdf'
    plt.savefig(
        path,
        bbox_inches='tight',
        pad_inches=0.05
    )
    # plt.show()
    plt.clf()


#成本
def cost_etc_details_plot(id,name,iftest8SendRate=""):

            ##cout<<Simulator::Now().GetNanoSeconds()<<" middle_value_for_plot: " <<" port: "<< port <<" CurrentCycle(T'th): "<< T_seq[port][priority][qIndex] \
                ##   << " newT[T+1](ns): "<< new_T << " newU[T+1]" << " Usram: "<<U_sram<<" Udram: "<<U_dram \
                ##   << " lambda: " <<ewma_r <<"  miu(Gbps): "<<Cqs <<"  Sr(MB): "<<Sr*1e-6<<"  Dr(Gbps): "<<Dr \
                ##   <<"  U_star[T]: "<<U_star<< "  U[T]: "<< last_utility \
                ##   <<" Storing_decision(0片外-1片内-2丢包): "<<storing_decision <<" final_dicision "<<final_dicision \
                ##   <<endl;

    file_pre = 'hybrid-buffer-test-'+id #文件前缀
    save_png_name1 = 'Period-test-'+id # newT[T+1]  Usram Udram  storing_decision  final_dicision
                                                    # y1轴 单位us   y2轴 log           y3轴 0/1/2   打点
    #save_png_name2 = 'Utility-test-'+id # U_star[T] U[T]
    save_png_name_U = 'U_Star&U-test-'+id
#
    data_dir = os.path.join(
        DATA_DIR,
        name,
        id,
        iftest8SendRate
    ) + os.sep

    save_path = FIG_DIR + os.sep

    file_name = data_dir + file_pre + '.txt' #....../hybPrid-buffer-test-tc2-01.txt


    with open(file_name) as file:
        

        time_sram = []
        time_dram = []
        time=[]
        time1=[]
        Us1 = []
        Us2 = []
        Ud1 = []
        Ud2 = []
        newT_Tplus11=[]
        newT_Tplus1=[]

        storing_decision_sram = []
        storing_decision_dram = []
        final_decision = []

        time1_sram = []
        time1_dram = []
        Us11 = []
        Us21 = []
        Ud11 = []
        Ud21 = []

        times_u0 = []
        u_star_values0 = []
        u_values0 = []
        times_u1 = []
        u_star_values1 = []
        u_values1 = []
        times_u2 = []
        u_star_values2 = []
        u_values2 = []

        storing_decision1_sram = []
        storing_decision1_dram = []
        final_decision1 = []

        md0 = []
        md1 = []
        md2 = []
        for line in file:
            if "middle_value_for_plot" not in line:
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
                times_u0.append((float(data[0]))/1000000.0) #ns->ms
                u_star_values0.append(float(data[22]))
                u_values0.append(float(data[24]))
                md0.append(float(data[40]))
                
                time.append((float(data[0]))/1000000.0) #ns->ms
                newT_Tplus1.append(float(data[7])/1000.0) #单位转us

                Us1.append(float(data[32]))
                Us2.append(float(data[34]))
                Ud1.append(float(data[36]))
                Ud2.append(float(data[38]))
                if int(data[26])==1:
                    storing_decision_sram.append(int(data[26]))
                    time_sram.append((float(data[0]))/1000000.0)
                else:
                    storing_decision_dram.append(int(data[26]))
                    time_dram.append((float(data[0]))/1000000.0)
                final_decision.append(int(data[28]))
                #print((float(data[0])-1e8)/1000000.0, float(data[7])/1000.0, int(data[26]))
            elif int(data[3]) == 1:
                #port1
                times_u1.append((float(data[0]))/1000000.0) #ns->ms
                u_star_values1.append(float(data[22]))
                u_values1.append(float(data[24]))
                md1.append(float(data[40]))

                time1.append((float(data[0]))/1000000.0) #ns->ms
                newT_Tplus11.append(float(data[7])/1000.0) #单位转us
            
                Us11.append(float(data[32]))
                Us21.append(float(data[34]))
                Ud11.append(float(data[36]))
                Ud21.append(float(data[38]))

                if int(data[26])==1:
                    storing_decision1_sram.append(int(data[26]))
                    time1_sram.append((float(data[0]))/1000000.0)
                else:
                    storing_decision1_dram.append(int(data[26]))
                    time1_dram.append((float(data[0]))/1000000.0)
                final_decision1.append(int(data[28]))
                #print((float(data[0])-1e8)/1000000.0, float(data[7])/1000.0, int(data[26]))
            elif int(data[3]) == 2:
                times_u2.append((float(data[0]))/1000000.0) #ns->ms
                u_star_values2.append(float(data[22]))
                u_values2.append(float(data[24]))
                md2.append(float(data[40]))

        #画port0
        #创建图表对象
        # ax1 = host_subplot(111, axes_class=axisartist.Axes)
        ax1 = plt.gca()
        #plt.subplots_adjust(right=0.75)

        ax2 = ax1.twinx()
        ax3 = ax1.twinx()

        # # #设置第三根Y轴位置，这里选择是右边
        # ax3.axis["right"] = ax3.new_fixed_axis(loc="right", offset=(60, 0))
        # ax2.axis["right"].toggle(all=True)
        # ax3.axis["right"].toggle(all=True)
        ax3.get_yaxis().set_visible(False)  #y轴不显示

        #第一个y轴
        ax1.plot(time,newT_Tplus1,label='T',color='k',marker='^')
        ax1.plot([],[],label=' ',color='w')#填充一个图例，使图例顺序好看点

        ax1.set_xlabel('Times (ms)',fontsize=font_size*0.8)
        ax1.set_ylabel('Scope (us)',fontsize=font_size*0.8)
        ax1.tick_params(labelsize=font_size*0.8,direction='in')
        ax1.xaxis.set_major_locator(MultipleLocator(0.2))

        # plt.xlim(x_start, x_end*2)

        #绘制第2个y轴的柱状图 Usram Udram log缩放
        ax2.tick_params(labelsize=font_size*0.8,direction='in')


        ax2.plot(time,Us1,label='U$_1$'+'$^S$',color='g',linestyle='-')
        ax2.plot(time,Ud1,label='U$_1$'+'$^D$',color='r',linestyle='-')
        ax2.plot(time,Us2,label='U$_2$'+'$^S$',color='g',linestyle='--')
        ax2.plot(time,Ud2,label='U$_2$'+'$^D$',color='r',linestyle='--')

        ax2.set_ylabel('Utility',fontsize=font_size*0.8)
        #ax2.set_yscale('log')
        #ax2.set_ylim(0,2000)

        #绘制第3个y轴的柱状图 storing_decision  final_dicision 打点
        plt.rcParams['lines.markersize'] = 4
        ax3.set_ylim(-0.5,2.5)
        #ax3.yaxis.set_major_locator(MultipleLocator(1))
        #ax3.scatter(time,final_decision,label='final_decision',color='black',s=80)
        ax3.scatter(time_sram,storing_decision_sram,label='SRAM',color='g', s=50)
        ax3.scatter(time_dram,storing_decision_dram,label='DRAM',color='r', s=50)
        #ax3.set_ylabel('OffChip-0 / OnChip-1 / Drop-2')

        # ax1.legend()
        #获取子图的句柄和标签
        lines1, labels1 = ax1.get_legend_handles_labels()
        lines2, labels2 = ax2.get_legend_handles_labels()
        lines3, labels3 = ax3.get_legend_handles_labels()
        plt.legend(lines1+lines2+lines3, labels1+labels2+labels3,loc=(0, 1),ncol=4)
        legend =plt.gca().get_legend()
        legend.get_frame().set_linewidth(0)
        #ax1.legend(bbox_to_anchor=(0, 1.01), loc=3, borderaxespad=0,fontsize='x-small',ncol=5)
        #plt.rcParams.update({'font.size': 10})
        plt.tight_layout()
        plt.savefig(save_path +name+'/'+id+'/'+iftest8SendRate+'/'+ save_png_name1+'-port0'+'.png')
        plt.clf()

        #画port1
        #创建图表对象
        ax1 = plt.gca()

        ax2 = ax1.twinx()
        ax3 = ax1.twinx()

        

        # # #设置第三根Y轴位置，这里选择是右边
        # ax3.axis["right"] = ax3.new_fixed_axis(loc="right", offset=(60, 0))
        # ax2.axis["right"].toggle(all=True)
        # ax3.axis["right"].toggle(all=True)

        ax3.get_yaxis().set_visible(False)  #y轴不显示

        #第一个y轴
        ax1.plot(time1,newT_Tplus11,label='T',color='k',marker='^')
        ax1.plot([],[],label=' ',color='w')#填充一个图例，使图例顺序好看点

        ax1.set_xlabel('Times (ms)',fontsize=font_size*0.8)
        ax1.set_ylabel('Scope (us)',fontsize=font_size*0.8)
        ax1.tick_params(labelsize=font_size*0.8,direction='in')
        ax1.xaxis.set_major_locator(MultipleLocator(0.2))


        #plt.xlim(0.4, 0.441)
        # plt.xlim(x_start, x_end*2)

        #绘制第2个y轴的柱状图 Usram Udram log缩放
        ax2.tick_params(labelsize=font_size*0.8,direction='in')
        ax2.plot(time1,Us11,label='U$_1$'+'$^S$',color='g',linestyle='-')
        ax2.plot(time1,Ud11,label='U$_1$'+'$^D$',color='r',linestyle='-')

        ax2.plot(time1,Us21,label='U$_2$'+'$^S$',color='g',linestyle='--')

        ax2.plot(time1,Ud21,label='U$_2$'+'$^D$',color='r',linestyle='--')
        ax2.set_ylabel('Utility',fontsize=font_size*0.8)
        #ax2.set_yscale('log')
        #ax2.set_ylim(0,2000)

        #绘制第3个y轴的柱状图 storing_decision  final_dicision 打点
        plt.rcParams['lines.markersize'] = 4
        # ax3.set_ylim(0,3)
        ax3.set_ylim(-0.5,2.5)
        ax3.scatter(time1_sram,storing_decision1_sram,label='SRAM',color='g', s=50)
        ax3.scatter(time1_dram,storing_decision1_dram,label='DRAM',color='r', s=50)
    
        # ax3.yaxis.set_major_locator(MultipleLocator(1))
        #ax3.scatter(time1,final_decision1,label='final_decision',color='black', s=80)
        # ax3.scatter(time1,storing_decision1,label='storing_decision',color='y', s=50)
        # ax3.set_ylabel('OffChip-0 / OnChip-1 / Drop-2')
        
        lines1, labels1 = ax1.get_legend_handles_labels()
        lines2, labels2 = ax2.get_legend_handles_labels()
        lines3, labels3 = ax3.get_legend_handles_labels()
        plt.legend(lines1+lines2+lines3, labels1+labels2+labels3,loc=(0, 1),ncol=4)
        legend =plt.gca().get_legend()
        legend.get_frame().set_linewidth(0)
        #ax1.legend(bbox_to_anchor=(0, 1.01), loc=3, borderaxespad=0,fontsize='x-small',ncol=5)
        
        plt.tight_layout()
        plt.savefig(save_path +name+'/'+id+'/'+iftest8SendRate+'/'+ save_png_name1+'-port1'+'.png')
        plt.clf()

        plt.rcParams['lines.markersize'] = 4
        if len(times_u0) > 0:
            line1, =plt.plot(times_u0, u_star_values0, marker='^', linestyle='-', color='b')
            line2, =plt.plot(times_u0, u_values0, marker='3', linestyle='-', color='g')
            line3, = plt.plot(times_u0,md0,marker = '+', linestyle='-', color='red')
            plt.xlabel("Time(ms)",fontsize=font_size*0.8)
            plt.ylabel("Utility and MD",fontsize=font_size*0.8)
            plt.legend(bbox_to_anchor=(0, 1.01), loc=3, borderaxespad=0,handles=[line1,line2,line3],labels=["U_Star","U","MD"],ncol=3,fontsize=10)
            legend =plt.gca().get_legend()
            legend.get_frame().set_linewidth(0)
            path = save_path + name +'/' +id + '/'+iftest8SendRate+'/'+save_png_name_U + '-port0.png'
            plt.savefig(path)
            plt.clf()
        if len(times_u1) > 0:
            line1, =plt.plot(times_u1, u_star_values1, marker='^', linestyle='-', color='b')
            line2, =plt.plot(times_u1, u_values1, marker='3', linestyle='-', color='g')
            line3, = plt.plot(times_u1,md1,marker = '+', linestyle='-', color='red')
            plt.xlabel("Time(ms)",fontsize=font_size*0.8)
            plt.ylabel("Utility and MD",fontsize=font_size*0.8)
            plt.legend(bbox_to_anchor=(0, 1.01), loc=3, borderaxespad=0,handles=[line1,line2,line3],labels=["U_Star","U","MD"],ncol=3,fontsize=10)
            legend =plt.gca().get_legend()
            legend.get_frame().set_linewidth(0)
            path = save_path + name +'/' +id + '/'+iftest8SendRate+'/'+save_png_name_U + '-port1.png'
            plt.savefig(path)
            plt.clf()
        if len(times_u2) > 0:
            line1, =plt.plot(times_u2, u_star_values2, marker='^', linestyle='-', color='b')
            line2, =plt.plot(times_u2, u_values2, marker='3', linestyle='-', color='g')
            line3, = plt.plot(times_u2,md2,marker = '+', linestyle='-', color='red')
            plt.xlabel("Time(ms)",fontsize=font_size*0.8)
            plt.ylabel("Utility and MD",fontsize=font_size*0.8)
            plt.legend(bbox_to_anchor=(0, 1.01), loc=3, borderaxespad=0,handles=[line1,line2,line3],labels=["U_Star","U","MD"],ncol=3,fontsize=10)
            legend =plt.gca().get_legend()
            legend.get_frame().set_linewidth(0)
            path = save_path + name +'/' +id + '/'+iftest8SendRate+'/'+save_png_name_U + '-port2.png'
            plt.savefig(path)
            plt.clf()

        
def T_AI_MD_details(id,name,iftest8SendRate=""):
    file_pre = 'hybrid-buffer-test-'+id 
    data_dir = os.path.join(
        DATA_DIR,
        name,
        id,
        iftest8SendRate
    ) + os.sep

    save_path = FIG_DIR + os.sep
    file_name = data_dir + file_pre + '.txt'

    with open(file_name) as file:
        time = []
        AI=[]
        MD=[]
        dealt_U=[]
        T = []

        time1 = []
        AI1=[]
        MD1=[]
        dealt_U1=[]
        T1 = []

        port =-1
        value=26 #让MD和U乘上value，使得MD和AI数值范围接近，能够使用同一个y轴

        lines=file.readlines()
        for line in lines:
            line=line.split()
            if 'states_in_end_of_period' in line:
                port=float(line[4])
            if 'New_Period' not in line:
                continue
            time0=float(line[0])/1000000
            AI0=float(line[12])/1000
            MD0=float(line[14])
            T0=float(line[2][6:])/1000
            U0=float(line[18])

            if port==0:
                time.append(time0)
                AI.append(AI0)
                MD.append(MD0)
                T.append(T0)
                dealt_U.append(U0)
            elif port==1:
                time1.append(time0)
                AI1.append(AI0)
                MD1.append(MD0*value)
                T1.append(T0)
                dealt_U1.append(U0*value)

        #port0
        plt.xlim(-0.1,1.5)
        plt.tick_params(axis='x',direction = 'in')
        plt.xticks(fontsize = final_font_size) 
        plt.xlabel('Time(ms)',fontsize=final_font_size)

        plt.tick_params(axis='y',direction = 'in')
        plt.yticks(fontsize = final_font_size)
        plt.ylabel('T and AI (us)',fontsize=final_font_size)

        plt.plot(time,T,label='T',linewidth=2*linewidth,marker='s',color='k')
        plt.plot(time,AI,label='AI',linewidth=2*linewidth,marker='^',color='g')

        ax=plt.gca()
        #ax.xaxis.set_major_locator(MultipleLocator(0.2))
        # ax.yaxis.set_major_locator(MultipleLocator(1))

        ax2=ax.twinx()
        ax2.set_ylim(-0.1,1.1)
        ax2.tick_params(labelsize=final_font_size,direction='in')
        ax2.yaxis.set_major_locator(MultipleLocator(0.2))

        ax2.plot(time, MD, label='MD',linewidth=2*linewidth,marker='d',color='r')
        ax2.plot(time,dealt_U,label='ΔU',linewidth=2*linewidth,marker='o',color='b')
        ax2.set_ylabel('MD and ΔU', fontsize=final_font_size)

        handles1,labels1=ax.get_legend_handles_labels()
        handles2,labels2=ax2.get_legend_handles_labels()

        plt.subplots_adjust(left=0.15, right=0.85, top=0.85,bottom=0.17) #调整子图相对于图形边缘的位置
        plt.legend(handles1+handles2,labels1+labels2,loc=(0,1),ncol=4,fontsize=final_font_size,handlelength=1.2, columnspacing=0.7,handletextpad=0.2)


        legend = plt.gca().get_legend()
        legend.get_frame().set_linewidth(0)

        save=save_path+name+'/'+id+'/'+iftest8SendRate+'/'+'T_AI_MD_details-'+id+'-port0'+'.pdf'
        plt.savefig(save)
        plt.clf()

        #port1
        #fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 5))  

        bax=brokenaxes(xlims=((0.2116,0.23),(1.4016,1.428)),hspace=.5)

        bax.set_ylim(-0.5, 4.2)

        bax.tick_params(labelsize=final_font_size,direction = 'in')
        bax.set_xlabel('Time(ms)',fontsize=final_font_size, labelpad=30)
        bax.tick_params(labelsize=final_font_size,direction = 'in')
        bax.set_ylabel('T and AI (us)',fontsize=final_font_size)

        bax.plot(time1,T1,label='T',linewidth=2*linewidth,marker='s',color='k')
        bax.plot(time1,AI1,label='AI',linewidth=2*linewidth,marker='^',color='g')
        
        bax.plot(time1, MD1, label='MD',linewidth=2*linewidth,marker='d',color='r')
        bax.plot(time1,dealt_U1,label='ΔU',linewidth=2*linewidth,marker='o',color='b')


        ax2=plt.gca().twinx()
        ax2.set_ylim(-0.1,1.1)

        ax2.tick_params(labelsize=final_font_size,direction='in')
        ax2.yaxis.set_major_locator(MultipleLocator(0.2))

        #填充图例，因为bax.plot不会产生图例
        ax2.plot([],[],label='T',linewidth=2*linewidth,marker='s',color='k')
        ax2.plot([],[],label='AI',linewidth=2*linewidth,marker='^',color='g')

        ax2.plot([],[], label='MD',linewidth=2*linewidth,marker='d',color='r')
        ax2.plot([],[],label='ΔU',linewidth=2*linewidth,marker='o',color='b')
        ax2.set_ylabel('MD and ΔU', fontsize=final_font_size)

        handles2,labels2=ax2.get_legend_handles_labels()

        plt.subplots_adjust(top=0.89,left=0.12,right=0.85,bottom=0.15)
        plt.legend(loc=(0,1),ncol=4,fontsize=final_font_size,handlelength=1.2, columnspacing=0.7,handletextpad=0.2)


        legend = plt.gca().get_legend()
        legend.get_frame().set_linewidth(0)

        save=save_path+name+'/'+id+'/'+iftest8SendRate+'/'+'T_AI_MD_details-'+id+'-port1'+'.pdf'
        plt.savefig(save)
        plt.clf()




def get_last_field(file_name):
    with open(file_name, 'rb') as f:
        f.seek(-2, 2)  # 移动到倒数第二个字节
        while f.read(1) != b'\n':  # 读到换行符为止
            f.seek(-2, 1)  # 继续向前移动
        last_line = f.readline().decode()  # 读取最后一行
    return last_line.strip().split(',')[-1]  # 获取最后一项


def test8_plot(id,name):
    final_font_size = 30
    # Deephir
    # 定义文件路径
    flow_rates = [100,200,300,400,500,600,700,800,900]
    thresholds = [0.2,0.5,1.0,2.0]  # ,4.0
    plt.figure()
    markers_BMS = ['o','v','^','s','*']
    colors_BMS = ['green','blue','purple','k','orange']
    file_path_pre = data_dir + 'BMS/tc2-08/'
    i = 0
    lines = [0,0,0,0,0,0]

    plt.figure().set_figheight(4)
    # plt.yscale("symlog")
    plt.figure(dpi=600,figsize=(12, 6))

    for threshold in thresholds:
        y = []
        for flow_rate in flow_rates:
            file_path = file_path_pre + str(threshold) +'M/' + str(flow_rate) +'/loss_packet.csv'
            # 打开文件并读取最后一行的最后一项
            loss_num = float(get_last_field(file_path)) /1000
            y.append(loss_num)
        epsilon = 1e-15  # 设置一个极小值
        y[y==0] = epsilon
        # 绘制折线图
        lines[i], =plt.plot(flow_rates, y, marker=markers_BMS[i], linestyle='-', color=colors_BMS[i],linewidth =linewidth_middle+2, markersize=10)
        i = i+1
    # pbs
    file_path_pre = data_dir + 'pbs/tc2-08/'
    y = []
    for flow_rate in flow_rates:
        file_path = file_path_pre + str(flow_rate) +'/loss_packet.csv'
        # 打开文件并读取最后一行的最后一项
        loss_num = float(get_last_field(file_path)) / 1000
        y.append(loss_num)

    # 绘制折线图
    lines[i], =plt.plot(flow_rates, y, marker='>', linestyle='-', color='r',linewidth =linewidth_middle+2, markersize=10)
    plt.xlabel("Flow Rate(Gbps)",fontsize=final_font_size)
    plt.ylabel("# of Packet Loss (x1e3)",fontsize=final_font_size)
    plt.xticks(fontsize=final_font_size)  # 调整x轴刻度大小
    plt.yticks(fontsize=final_font_size)  # 调整y轴刻度大小

    plt.legend(bbox_to_anchor=(-0.02, 1.001), loc=3, columnspacing=0.5,borderaxespad=0,handles=lines,labels=["DeepHir-0.2M","DeepHir-0.5M","DeepHir-1M","DeepHir-2M","FBM"],ncol=3,fontsize=final_font_size, handlelength=1.5, handletextpad=0.1)
    legend =plt.gca().get_legend()  # "DeepHir-4M",
    legend.get_frame().set_linewidth(0)
    path = save_path+'compare/tc2-08/adaptability-to-Traffic-Variation-compare.pdf'
    plt.savefig(path, bbox_inches='tight')
    plt.clf()


def test8_new_plot(testcase_number, testcase_name):

    final_font_size = 30
    # 新实验的自变量：500Gbps背景流持续时间，单位us
    change_times_us = [5,10, 15, 30, 45, 60]  # 5 10 15 30 45 60
    # DeepHiR静态阈值
    thresholds = [0.2, 0.5, 1.0, 2.0]  # , 4.0
    markers_bms = ['o', 'v', '^', 's', '*']
    colors_bms = ['green', 'blue', 'purple', 'k', 'orange']
    legend_labels = [
        "DeepHiR-0.2M",
        "DeepHiR-0.5M",
        "DeepHiR-1M",
        "DeepHiR-2M",
        # "DeepHiR-4M",
        "FBM"
    ]

    fig, ax = plt.subplots(dpi=600,  figsize=(12, 6))
    lines = []

    bms_path_prefix = os.path.join( data_dir, "BMS", testcase_number )

    for index, threshold in enumerate(thresholds):
        loss_values = []
        for change_time_us in change_times_us:
            file_path = os.path.join(
                bms_path_prefix,
                f"{threshold}M",
                f"{change_time_us}us",
                "loss_packet.csv"
            )

            if not os.path.isfile(file_path):
                raise FileNotFoundError(
                    f"未找到DeepHiR丢包文件：{file_path}"
                )

            loss_num = float(get_last_field(file_path))
            loss_values.append(loss_num)

        line, = ax.plot(
            change_times_us,
            loss_values,
            marker=markers_bms[index],
            linestyle='-',
            color=colors_bms[index],
            linewidth=linewidth_middle + 2,
            markersize=10
        )

        lines.append(line)

    pbs_path_prefix = os.path.join(  data_dir, "pbs", testcase_number )

    pbs_loss_values = []

    for change_time_us in change_times_us:
        file_path = os.path.join(
            pbs_path_prefix,
            f"{change_time_us}us",
            "loss_packet.csv"
        )
        if not os.path.isfile(file_path):
            raise FileNotFoundError(
                f"未找到FBM丢包文件：{file_path}"
            )

        loss_num = float(get_last_field(file_path))
        pbs_loss_values.append(loss_num)

    line, = ax.plot(
        change_times_us,
        pbs_loss_values,
        marker='>',
        linestyle='-',
        color='r',
        linewidth=linewidth_middle + 2,
        markersize=10
    )
    lines.append(line)
    ax.set_xlabel("High-rate Background Duration x (μs)", fontsize=final_font_size)
    ax.set_ylabel("# of Packet Loss",fontsize=final_font_size)

    # 明确设置横轴刻度
    ax.set_xticks(change_times_us)

    ax.tick_params( axis='x',labelsize=final_font_size)
    ax.tick_params(axis='y', labelsize=final_font_size)

    legend = ax.legend(
        bbox_to_anchor=(-0.02, 1.001),
        loc=3,
        columnspacing=0.5,
        borderaxespad=0,
        handles=lines,
        labels=legend_labels,
        ncol=3,
        fontsize=final_font_size,
        handlelength=1.5,
        handletextpad=0.1
    )
    legend.get_frame().set_linewidth(0)

    output_dir = os.path.join(save_path,"compare", testcase_number )
    os.makedirs(output_dir, exist_ok=True)
    output_path = os.path.join(
        output_dir,
        "adaptability-to-background-duration-compare-new.pdf"
    )
    fig.savefig(
        output_path,
        bbox_inches='tight'
    )
    plt.close(fig)
    print(f"图片已保存：{output_path}")


################################################################################################################################################
def test9_plot():
    # Deephir
    # 定义文件路径
    # flow_rates = [100,200,300,400,500,600,700,800,900]
    thresholds = [0.2,1.0,2.0,3.0,4.0]
    testcase='tc2-09'
    print("test9_plot被调用，绘制所有需要的图【包含BMS，pbs】")

    print(f'testcase={testcase}')

    # try:
    #     print("开始尝试BMS")
    #     algorithm='BMS'
    #     for i in thresholds:
    #         i=str(i)
    #         print(f'threshold={i}')
    #         temp=testcase+'/'+i+'M'
    #         queue_write_read_throughput_plot(temp,algorithm)
    #         print('queue_write_read_throughput_plot'+'运行良好')
    #         queue_usage_plot(temp,algorithm)
    #         print('queue_usage_plot'+'运行良好')            
    #         cost_etc_details_plot(temp,algorithm)
    #         print('cost_etc_details_plot'+'运行良好')   
    #         lambda_cost_etc_details_plot(temp,algorithm)
    #         print('lambda_cost_etc_details_plot'+'运行良好')   
    # except Exception as e:
    #     print("BMS这里出问题了")
    # finally:
    #     print("BMS部分完结")
    
    # try:
    #     algorithm='pbs'
    #     queue_write_read_throughput_plot(testcase,algorithm)
    #     queue_usage_plot(testcase,algorithm)
    #     cost_etc_details_plot(testcase,algorithm)
    #     lambda_cost_etc_details_plot(testcase,algorithm)
    # except Exception as e:
    #     print("pbs这里出问题了")
    # finally:
    #     print("pbs部分完结")
    

    x_data = ['0.2M','1.0M','2.0M','3.0M','4.0M','pbs']
    ax_data = []#丢包loss
    ax1_data = []#吞吐
    file_path_pre = data_dir + 'BMS/tc2-09/'



    for threshold in thresholds:
        # for flow_rate in flow_rates:
        file_path = file_path_pre + str(threshold) +'M/loss_packet.csv'
        # 打开文件并读取最后一行的最后一项
        loss_num = float(get_last_field(file_path))
        ax_data.append(loss_num)
        file_path = file_path_pre + str(threshold) +''

    #pbs
    file_path_pre = data_dir + 'pbs/tc2-09/'
    # for flow_rate in flow_rates:
    file_path = file_path_pre  +'loss_packet.csv'
    # 打开文件并读取最后一行的最后一项
    loss_num = float(get_last_field(file_path))
    ax_data.append(loss_num)
##############################################################LOSS收集完毕

    #thrput_path=



    fig=plt.figure()#dpi=600
    ax=fig.add_subplot(111)

    ax.set_ylim([15,35])
    ax.set_yticks=np.arange(15,35)
    ax.set_yticklabels=np.arange(15,35)

    bar_width = 0.3
    ax.set_ylabel('this is y1',fontsize=18,fontweight='bold');
    lns1=ax.bar(x=np.arange(len(x_data)), width=bar_width, height=ax_data, label='y1', fc = 'steelblue',alpha=0.8)

    for a,b in enumerate(ax_data):
        plt.text(a,b+0.0005,'%s' % b,ha='center')
        
        
    ax1 = ax.twinx() # this is the important function   

    ax1.set_ylim([6,10])
    ax1.set_yticks=np.arange(6,10)
    ax1.set_yticklabels=np.arange(6,10)
    ax1.set_ylabel('this is y2',fontsize=18,fontweight='bold');
    lns2=ax1.bar(x=np.arange(len(x_data))+bar_width, width=bar_width, height=ax1_data,label='y2',fc = 'indianred',alpha=0.8)

    for a,b in enumerate(ax1_data):
        plt.text(a+0.3,b+0.001,'%s' % b,ha='center')
        
    plt.xticks(np.arange(len(x_data))+bar_width/2, x_data)

    ax.set_xlabel('double Y',fontsize=20,fontweight='bold')

    fig.legend(loc=1,bbox_to_anchor=(0.28,1),bbox_transform=ax.transAxes)


##########################################################################################################################################
    plt.figure()
    markers_BMS = ['^','3','P','x','_']
    colors_BMS = ['b','g','r','m','y']
    file_path_pre = data_dir + 'BMS/tc2-09/'
    i = 0
    lines = [0,0,0,0,0,0]
    plt.rcParams['lines.markersize'] = 4
    for threshold in thresholds:
        y = []
        # for flow_rate in flow_rates:
        file_path = file_path_pre + str(threshold) +'M/loss_packet.csv'
        # 打开文件并读取最后一行的最后一项
        loss_num = float(get_last_field(file_path))
        y.append(loss_num)
        # 绘制柱状图
        lines[i], =plt.plot([0], y, marker=markers_BMS[i], linestyle='-', color=colors_BMS[i])#########################################
        i = i+1
    # pbs
    file_path_pre = data_dir + 'pbs/tc2-09/'
    y = []
    # for flow_rate in flow_rates:
    file_path = file_path_pre  +'loss_packet.csv'
    # 打开文件并读取最后一行的最后一项
    loss_num = float(get_last_field(file_path))
    y.append(loss_num)
    # 绘制折线图
    lines[i], =plt.plot([0], y, marker='>', linestyle='-', color='orange')######################################################
    plt.xlabel("Flow Rate(Gbps)",fontsize=font_size*0.8)
    plt.ylabel("Loss Num",fontsize=font_size*0.8)
    plt.legend(bbox_to_anchor=(0, 1.01), loc=3, borderaxespad=0,handles=lines,labels=["Deephir-0.2M","Deephir-1M","Deephir-2M","Deephir-3M","Deephir-4M","FBM"],ncol=3,fontsize=10)
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    path = save_path+'compare/tc2-09/adaptability-to-Traffic-Variation-compare.png'
    plt.savefig(path)
    plt.clf()

def lambda_cost_etc_details_plot(id,name,iftest8SendRate=""):
    file_pre = 'hybrid-buffer-test-'+id #文件前缀
    save_png_name1 = 'Period-test-lambda-miu-'+id # newT[T+1]  Usram Udram  storing_decision  final_dicision
    case_data_dir = os.path.join(
        DATA_DIR,
        name,
        id,
        iftest8SendRate
    )

    case_save_dir = os.path.join(
        FIG_DIR,
        name,
        id,
        iftest8SendRate
    )

    os.makedirs(case_save_dir, exist_ok=True)

    file_name = os.path.join(
    case_data_dir,
    file_pre + ".txt"
    )
    with open(file_name) as file:
        time = []
        lambda0 =[]
        miu0 = []

        time1 = []
        lambda1 = []
        miu1 = []

        for line in file:
            if "middle_value_for_plot:" not in line:
                #print("skip this line")
                continue
            data = line.strip().split()

             #data:110912148 10912148ns
            #['100001866', 'middle_value_for_plot:', 'port:', '0', "CurrentCycle(T'th):", '1', 'newT[T+1](ns):', '8388', 'newU[T+1]', 'Usram:', '1.333', 'Udram:', '1.18475', 'lambda:', '898.72', 'miu(Gbps):', '778.56', 'Sr(MB):', '4.1943', 'Dr(Gbps):', '1000', 'U_star[T]:', '0.999999', 'U[T]:', '0', 'Storing_decision(0片外-1片内-2丢包):', '1', 'final_dicision', '1']
            #port0
            if int(data[3]) == 0: 
                time.append((float(data[0]))/1000000.0) #ns->ms
                lambda0.append(float(data[14]))
                miu0.append(float(data[16]))
            elif int(data[3]) == 1:
                #port1
                time1.append((float(data[0]))/1000000.0) #ns->ms
                lambda1.append(float(data[14]))
                miu1.append(float(data[16]))
               
        
        #创建图表对象
        ax1 = host_subplot(111, axes_class=axisartist.Axes)
        plt.subplots_adjust(right=0.75)

        ax2 = ax1.twinx()
        ax2.axis["right"].toggle(all=True)

        
        #第一个y轴
        ax1.plot(time,lambda0,label='port0 λ',color='r',linestyle='-')
        ax1.plot(time1,lambda1,label='port1 λ',color='r',linestyle='--')
        ax1.set_xlabel('Times (ms)')
        ax1.set_ylabel('lambda')

        # plt.xlim(x_start, x_end)

        #第二个y轴
        ax2.plot(time,miu0,label='port0 μ',color='g',linestyle='-')
        ax2.plot(time1,miu1,label='port1 μ',color='g',linestyle='--')
        ax2.set_ylabel('miu')

        # ax1.legend()
        ax1.legend(bbox_to_anchor=(0, 1.01), loc=3, borderaxespad=0,fontsize='x-small',ncol=5)
        plt.rcParams.update({'font.size': 10})
        plt.tight_layout()
        save = os.path.join(
            case_save_dir,
            f"T_AI_MD_details-{id}-port0.pdf"
        )

        plt.savefig(save)
        plt.clf()#清除图像

def U2_U2star_details_plot(id):
    file_name =data_dir+'pbs'+'/'+id+'/'+'hybrid-buffer-test-'+id+'.txt'
    time = []
    Qis = []
    Sr = []
    Dr = []
    T = []
    U2 = []
    U2_ = []
    lambda0 = []
    Cin = []
    Cout = []
    in_out = []
    in_T = []

    calculated_Q = []

    with open(file_name) as file:
        lines =file.readlines()
        for line in lines:
            line=line.split()
            if '分析----' not in line:
                continue
            time0 =float(line[1][3:])/1000000.0
            choice =float(line[2][-1])
            lambda_ = float(line[4][7:])
            T0 = float(line[5][7:])
            Q0 =float(line[9][12:])
            Sr0 = float(line[11][3:])
            Dr0 =float(line[13][3:])
            CSin = float(line[15][7:])
            CSout=float(line[17][9:])
            U2_0 = float(line[18][7:])
            U20=float(line[19][3:])

            time.append(time0)
            U2.append(U20)
            U2_.append(U2_0)
            T.append(T0)
            Cin.append(CSin)
            calculated_Q.append(lambda_*T0-CSout)

            if choice ==0:
                Dr.append(Dr0)
                Sr.append(np.nan)
                lambda0.append(lambda_)
                Qis.append(np.nan)
                Cout.append(np.nan)
                in_out.append(np.nan)
                in_T.append(CSin/T0)
            else:
                Dr.append(np.nan)
                Sr.append(Sr0)
                lambda0.append(np.nan)
                Qis.append(Q0)
                Cout.append(CSout)
                in_out.append(CSin-CSout)
                in_T.append(np.nan)
    
    # C_i_in C_i_sout
    # plt.xlim(0,2)
    plt.xlabel('Time(ms)',fontsize=font_size*0.8)
    plt.tick_params(direction='in')
    plt.xticks(fontsize=font_size*0.8)
    plt.ylabel('',fontsize=font_size*0.8)
    plt.ylim(-1e5,5e6)
    plt.yticks(fontsize=font_size*0.8)

    ax=plt.gca()
    ax.xaxis.set_major_locator(MultipleLocator(0.4))
    ax.yaxis.set_major_locator(MultipleLocator(1e6))

    plt.plot(time,Sr,label='Sr',linewidth=linewidth,marker='*',color='g')
    # plt.plot(time,lambda0,label='λ/1e3',linewidth=linewidth,marker='d',color='gray')
    plt.plot(time,Cin,label='C_i_in',linewidth=linewidth,marker='o',color='c')
    plt.plot(time,Cout,label='C_i_sout',linewidth=linewidth,marker='^',color='orange')
    # plt.plot(time,in_out,label='(C_i_in - C_i_sout)/1e3',linewidth=linewidth,marker='p',color='purple')
    # plt.plot(time,in_T,label='(C_i_in / T)/1e3',linewidth=linewidth,marker='h',color='brown')


    # ax2=ax.twinx()
    # ax2.set_ylabel('Utility',fontsize=font_size*0.8)
    # ax2.set_ylim(-5,5)
    # ax2.tick_params(labelsize=font_size*0.8,direction='in')

    # ax2.yaxis.set_major_locator(MultipleLocator(2))

    # ax2.plot(time,U2,label='U2',linewidth=linewidth,marker='v',color='y')
    # ax2.plot(time,U2_,label='U2*',linewidth=linewidth,marker='+',color='pink')

    
    # lines1, labels1 = ax.get_legend_handles_labels()
    # lines2, labels2 = ax2.get_legend_handles_labels()
    # plt.legend(lines1+lines2, labels1+labels2,loc=(-0.1,1.05),ncol=4,fontsize='medium')

    plt.legend(loc='upper right')
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(bottom=0.1,left=0.1) #调整子图相对于图形边缘的位置

    save=save_path+'pbs'+'/'+id+'/'+'C_i_in C_i_sout details'
    plt.savefig(save)
    plt.clf()


    
    #delta_Q_i_s Sr
    # plt.xlim(0,2)
    plt.xlabel('Time(ms)',fontsize=font_size*0.8)
    plt.tick_params(direction='in')
    plt.xticks(fontsize=font_size*0.8)
    plt.ylabel('',fontsize=font_size*0.8)
    plt.ylim(-1e5,5e6)
    plt.yticks(fontsize=font_size*0.8)

    ax=plt.gca()
    ax.xaxis.set_major_locator(MultipleLocator(0.4))
    ax.yaxis.set_major_locator(MultipleLocator(1e6))
    plt.plot(time,Sr,label='Sr',linewidth=linewidth,marker='*',color='g')
    plt.plot(time,Qis,label='delta_Q_i_s',linewidth=linewidth,marker='^',color='k')

    plt.legend(loc='upper right')
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(bottom=0.1,left=0.1) #调整子图相对于图形边缘的位置

    save=save_path+'pbs'+'/'+id+'/'+'delta_Q_i_s Sr details'
    plt.savefig(save)
    plt.clf()


    # C_i_in T Dr
    # plt.xlim(0,2)
    plt.xlabel('Time(ms)',fontsize=font_size*0.8)
    plt.tick_params(direction='in')
    plt.xticks(fontsize=font_size*0.8)
    plt.ylabel('C_i_in and T',fontsize=font_size*0.8)
    plt.ylim(-1e5,2e6)
    plt.yticks(fontsize=font_size*0.8)

    ax=plt.gca()
    ax.xaxis.set_major_locator(MultipleLocator(0.4))
    ax.yaxis.set_major_locator(MultipleLocator(5e5))

    plt.plot(time,Cin,label='C_i_in',linewidth=linewidth,marker='o',color='c')
    plt.plot(time,T,label='T',linewidth=linewidth,marker='*',color='red')

    ax2=ax.twinx()
    ax2.set_ylabel('Dr',fontsize=font_size*0.8)
    ax2.set_ylim(-100,2000)
    ax2.tick_params(labelsize=font_size*0.8,direction='in')
    ax2.yaxis.set_major_locator(MultipleLocator(500))
    ax2.plot(time,Dr,label='Dr',linewidth=linewidth,marker='^',color='b')

    lines1, labels1 = ax.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    plt.legend(lines1+lines2, labels1+labels2,loc='upper right')

    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(bottom=0.1,left=0.15,right=0.85) #调整子图相对于图形边缘的位置

    save=save_path+'pbs'+'/'+id+'/'+'C_i_in T Dr details'
    plt.savefig(save)
    plt.clf()

def lambda_Dr_details_plot(id,iftest8SendRate=""):
    file_name =data_dir+'pbs'+'/'+id+'/'+iftest8SendRate+'/'+'hybrid-buffer-test-'+id+'.txt'
    time = []
    Dr = []
    choice = []
    lambda0 = []
    with open(file_name) as file:
        lines =file.readlines()
        for line in lines:
            line=line.split()
            if '分析----' not in line:
                continue
            time0 =float(line[1][3:])/1000000.0
            choice =float(line[2][-1])
            lambda_ = float(line[4][7:])
            Dr0 =float(line[13][3:])
            time.append(time0)

            Dr.append(Dr0)
            lambda0.append(lambda_)

    
    # plt.xlim(0,2)
    plt.xlabel('Time(ms)',fontsize=font_size*0.8)
    plt.tick_params(direction='in')
    plt.xticks(fontsize=font_size*0.8)
    plt.ylabel('',fontsize=font_size*0.8)
    plt.yticks(fontsize=font_size*0.8)

    ax=plt.gca()
    #ax.xaxis.set_major_locator(MultipleLocator(0.2))

    plt.plot(time,Dr,label='Dr',linewidth=linewidth,marker='^',color='b')
    plt.plot(time,lambda0,label='λ',linewidth=linewidth,marker='d',color='gray')


    plt.legend(bbox_to_anchor=(0, 1.01), loc=3, borderaxespad=0,ncol=2,fontsize = 10)
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(bottom=0.1,left=0.1) #调整子图相对于图形边缘的位置

    save=save_path+'pbs'+'/'+id+'/'+iftest8SendRate+'/'+'lambda-Dr-detail.png'
    plt.savefig(save)
    plt.clf()


def U2_U2star_detail(id):
    file_name =data_dir+'pbs'+'/'+id+'/'+'hybrid-buffer-test-'+id+'.txt'

    time = []
    Us2 = []
    Us2_star = []
    Ud2 = []
    Ud2_star = []


    with open(file_name) as file:
        lines =file.readlines()
        for line in lines:
            line=line.split()
            if '分析----' not in line:
                continue
            time0 =float(line[1][3:])/1000000.0
            choice =float(line[2][-1])
            U2_0 = float(line[18][7:])
            U20=float(line[19][3:]) 

            time.append(time0)

            if choice == 1:
                Ud2.append(np.nan)
                Ud2_star.append(np.nan)
                Us2.append(U20)
                Us2_star.append(U2_0)
            else:
                Ud2.append(U20)
                Ud2_star.append(U2_0)
                Us2.append(np.nan)
                Us2_star.append(np.nan)
    
    # plt.xlim(0,2)
    plt.ylim(-0.8,0)
    plt.xlabel('Time(ms)',fontsize=font_size*0.8)
    plt.tick_params(direction='in')
    plt.xticks(fontsize=font_size*0.8)
    plt.ylabel('',fontsize=font_size*0.8)
    plt.yticks(fontsize=font_size*0.8)

    ax=plt.gca()
    #ax.xaxis.set_major_locator(MultipleLocator(0.2))
    ax.yaxis.set_major_locator(MultipleLocator(0.05))


    plt.plot(time,Ud2,label='U_D_2',linewidth=linewidth,marker='*',color='g')
    plt.plot(time,Ud2_star,label='U*_D_2',linewidth=linewidth,marker='^',color='b')
    plt.plot(time,Us2,label='U_S_2',linewidth=linewidth,marker='o',color='c')
    plt.plot(time,Us2_star,label='U*_S_2',linewidth=linewidth,marker='^',color='orange')

    plt.legend(bbox_to_anchor=(0, 1.01), loc=3, borderaxespad=0,ncol=4,fontsize = 10)
    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(bottom=0.1,left=0.1) #调整子图相对于图形边缘的位置

    save=save_path+'pbs'+'/'+id+'/'+'U2&U*2-detail.png'
    plt.savefig(save)
    plt.clf()

    #plt.plot(tiem,calculated_Q,label='',)

def delta_Q_detail(id,iftest8SendRate=""):
    file_name =data_dir+'pbs'+'/'+id+'/'+iftest8SendRate+'/'+'hybrid-buffer-test-'+id+'.txt'
    time = []
    T = []
    lambda0 = []
    Cout = []


    with open(file_name) as file:
        lines =file.readlines()
        for line in lines:
            line=line.split()
            if '分析----' not in line:
                continue
            time0 =float(line[1][3:])/1000000.0
            lambda_ = float(line[4][7:])
            T0 = float(line[5][7:])/1000.0
            CSout=float(line[17][9:])/1000.0

            time.append(time0)
            T.append(T0)

            lambda0.append(lambda_)
            Cout.append(CSout)
                
    
    # plt.xlim(0,2)
    
    plt.tick_params(direction='in')
    plt.xticks(fontsize=font_size*0.8)
    # plt.ylabel('',fontsize=font_size*0.8)
    # plt.yticks(fontsize=font_size*0.8)

    ax=plt.gca()
    #ax.xaxis.set_major_locator(MultipleLocator(0.2))
    ax.set_xlabel('Time(ms)',fontsize=font_size*0.8)

    zx = ax.twinx()
    lns1 = zx.plot(time,T,label='T(us)',linewidth=linewidth,marker='o',color='c')
    # plt.plot(time,T,label='T/1e3',linewidth=linewidth,marker='.',color='red')
    # plt.plot(time,Qis,label='delta_Q_i_s/1e3',linewidth=linewidth,marker='s',color='k')
    lns2 = ax.plot(time,lambda0,label='lambda',linewidth=linewidth,marker='*',color='g')
    lns3 = ax.plot(time,Cout,label='C_i_sout(/1000)',linewidth=linewidth,marker='^',color='orange')
    zx.set_ylabel('T(us)',fontsize=font_size*0.8)
    
    lns = lns1+lns2+lns3
    labs = [l.get_label() for l in lns]
    ax.legend(lns, labs, bbox_to_anchor=(0, 1.01), loc=3, borderaxespad=0,ncol=3,fontsize = 10)
    legend =ax.get_legend()
    legend.get_frame().set_linewidth(0)

    # legend =plt.gca().get_legend()
    # legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(bottom=0.1,left=0.1) #调整子图相对于图形边缘的位置

    save=save_path+'pbs'+'/'+id+'/'+iftest8SendRate+'/'+'delta_Q-details.png'
    plt.savefig(save)
    plt.clf()

    #plt.plot(tiem,calculated_Q,label='',)




def lambda_C_i_sout(id,iftest8SendRate=''):
    file_name =data_dir+'pbs'+'/'+id+'/'+iftest8SendRate+'/'+'hybrid-buffer-test-'+id+'.txt'
    # Qis = []
    # Sr = []
    # Dr = []
    # T = []
    # U2 = []
    # U2_ = []
    time0 =[]
    real_lambda0 = []
    predical_lambda0 = []
    real_C_S_out0 = []
    predical_C_S_out0 =[]

    time2=[]
    real_lambda2 = []
    predical_lambda2 = []
    real_C_S_out2 = []
    predical_C_S_out2 =[]

    # Cin = []
    # Cout = []
    # in_out = []
    # in_T = []
    # calculated_Q = []
    with open(file_name) as file:
        lines =file.readlines()
        for line in lines:
            line=line.split()
            if "port:" in line and 'states_in_end_of_period' in line:
                port=float(line[4])
            if '分析----' not in line:
                continue
            time_ =float(line[1][3:])/1000000.0
            #choice =float(line[2][-1])
            lambda_ = float(line[4][7:])
            T0 = float(line[5][7:])
            # Q0 =float(line[9][12:])
            # Sr0 = float(line[11][3:])
            # Dr0 =float(line[13][3:])
            CSin = float(line[15][7:])
            predical_CSout = float(line[7][7:])
            real_CSout=float(line[17][9:])
            # U2_0 = float(line[18][7:])
            # U20=float(line[19][3:])

            if port==0:
                time0.append(time_)
                real_lambda0.append(CSin/T0)
                predical_lambda0.append(lambda_)
                real_C_S_out0.append(real_CSout)
                predical_C_S_out0.append(predical_CSout)
            elif port==2:
                time2.append(time_)
                real_lambda2.append(CSin/T0)
                predical_lambda2.append(lambda_)
                real_C_S_out2.append(real_CSout)
                predical_C_S_out2.append(predical_CSout)
            
    
    plt.xlabel('Time(ms)',fontsize=font_size*0.8)
    plt.tick_params(direction='in')
    plt.xticks(fontsize=font_size*0.8)
    plt.ylabel('C_S_out',fontsize=font_size*0.8)
    if id =='tc2-05':
        plt.ylim(-1e5,2e6)
    else:
        plt.ylim(-1e5,5e6)
    plt.yticks(fontsize=font_size*0.8)

    ax=plt.gca()
    ax.xaxis.set_major_locator(MultipleLocator(0.4))
    ax.yaxis.set_major_locator(MultipleLocator(1e6))

    plt.plot(time0,real_C_S_out0,label='real_C_S_out',linewidth=linewidth,marker='^',color='orange')
    plt.plot(time0,predical_C_S_out0,label='predical_C_S_out',linewidth=linewidth,marker='p',color='k')

    ax2=ax.twinx()
    ax2.set_ylabel('lambda',fontsize=font_size*0.8)
    ax2.set_ylim(-10,300)
    ax2.tick_params(labelsize=font_size*0.8,direction='in')

    ax2.yaxis.set_major_locator(MultipleLocator(60))
    ax2.plot(time0,real_lambda0,label='real_lambda',linewidth=linewidth,marker='*',color='g')
    ax2.plot(time0,predical_lambda0,label='predical_lambda',linewidth=linewidth,marker='o',color='r')


    lines1, labels1 = ax.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    plt.subplots_adjust(top=0.8) #调整子图相对于图形边缘的位置
    plt.legend(lines1+lines2, labels1+labels2,loc=(0,1.05),ncol=2)


    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(bottom=0.1,left=0.1,right=0.85) #调整子图相对于图形边缘的位置

    save=save_path+'pbs'+'/'+id+'/'+iftest8SendRate+'/'+'lambda_C_i_sout details-port0'
    plt.savefig(save)
    plt.clf()


    plt.xlabel('Time(ms)',fontsize=font_size*0.8)
    plt.tick_params(direction='in')
    plt.xticks(fontsize=font_size*0.8)
    plt.ylabel('C_S_out',fontsize=font_size*0.8)
    if id =='tc2-05':
        plt.ylim(-1e5,2e6)
    else:
        plt.ylim(-1e5,5e6)
    plt.yticks(fontsize=font_size*0.8)

    ax=plt.gca()
    ax.xaxis.set_major_locator(MultipleLocator(0.4))
    ax.yaxis.set_major_locator(MultipleLocator(1e6))

    plt.plot(time2,real_C_S_out2,label='real_C_S_out',linewidth=linewidth,marker='^',color='orange')
    plt.plot(time2,predical_C_S_out2,label='predical_C_S_out',linewidth=linewidth,marker='p',color='k')

    ax2=ax.twinx()
    ax2.set_ylabel('lambda',fontsize=font_size*0.8)
    ax2.set_ylim(-10,300)
    ax2.tick_params(labelsize=font_size*0.8,direction='in')

    ax2.yaxis.set_major_locator(MultipleLocator(60))
    ax2.plot(time2,real_lambda2,label='real_lambda',linewidth=linewidth,marker='*',color='g')
    ax2.plot(time2,predical_lambda2,label='predical_lambda',linewidth=linewidth,marker='o',color='r')

    lines1, labels1 = ax.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    plt.subplots_adjust(top=0.8) #调整子图相对于图形边缘的位置
    plt.legend(lines1+lines2, labels1+labels2,loc=(0,1.05),ncol=2)


    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(bottom=0.1,left=0.1,right=0.85) #调整子图相对于图形边缘的位置


    save=save_path+'pbs'+'/'+id+'/'+iftest8SendRate+'/'+'lambda_C_i_sout details-port2'
    plt.savefig(save)
    plt.clf()


def Dr_Sr_Q_plot(id,iftest8SendRate=''):
    file_name =data_dir+'pbs'+'/'+id+'/'+iftest8SendRate+'/'+'hybrid-buffer-test-'+id+'.txt'
    time0 =[]
    predical_lambda0 = []
    predical_delta_Q0 =[]
    Dr0=[]
    Sr0=[]

    time2 =[]
    predical_lambda2 = []
    predical_delta_Q2 =[]
    Dr2=[]
    Sr2=[]

    with open(file_name) as file:
        lines =file.readlines()
        for line in lines:
            line=line.split()
            if "port:" in line and 'states_in_end_of_period' in line:
                port=float(line[4])
            if '分析----' not in line:
                continue
            time_ =float(line[1][3:])/1000000.0
            lambda_ = float(line[4][7:])
            Q =float(line[9][12:])
            Sr = float(line[11][3:])
            Dr =float(line[13][3:])

            if port==0:
                time0.append(time_)
                predical_delta_Q0.append(Q)
                predical_lambda0.append(lambda_)
                Dr0.append(Dr)
                Sr0.append(Sr)
            elif port==2:
                time2.append(time_)
                predical_delta_Q2.append(Q)
                predical_lambda2.append(lambda_)
                Dr2.append(Dr)
                Sr2.append(Sr)
            
    
    plt.xlabel('Time(ms)',fontsize=font_size*0.8)
    plt.tick_params(direction='in')
    plt.xticks(fontsize=font_size*0.8)
    plt.ylabel('delta_Q and Sr',fontsize=font_size*0.8)
    if id =='tc2-05':
        plt.ylim(-2e5,5e6)
    else:
        plt.ylim(-1e5,5e6)
    plt.yticks(fontsize=font_size*0.8)

    ax=plt.gca()
    ax.xaxis.set_major_locator(MultipleLocator(0.4))
    ax.yaxis.set_major_locator(MultipleLocator(1e6))

    plt.plot(time0,predical_delta_Q0,label='predical_delta_Q',linewidth=linewidth,marker='^',color='orange')
    plt.plot(time0,Sr0,label='Sr',linewidth=linewidth,marker='o',color='r')

    ax2=ax.twinx()
    ax2.set_ylabel('lambda and Dr',fontsize=font_size*0.8)
    ax2.set_ylim(-100,1200)
    ax2.tick_params(labelsize=font_size*0.8,direction='in')

    ax2.yaxis.set_major_locator(MultipleLocator(200))
    ax2.plot(time0,Dr0,label='Dr',linewidth=linewidth,marker='*',color='g')
    ax2.plot(time0,predical_lambda0,label='predical_lambda',linewidth=linewidth,marker='p',color='k')

    lines1, labels1 = ax.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    plt.legend(lines1+lines2, labels1+labels2,loc=(0,1.05),ncol=2)

    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(top=0.8,bottom=0.1,left=0.1,right=0.85) #调整子图相对于图形边缘的位置

    save=save_path+'pbs'+'/'+id+'/'+iftest8SendRate+'/'+'Dr_Sr_Q-port0'
    plt.savefig(save)
    plt.clf()


    plt.xlabel('Time(ms)',fontsize=font_size*0.8)
    plt.tick_params(direction='in')
    plt.xticks(fontsize=font_size*0.8)
    plt.ylabel('delta_Q and Sr',fontsize=font_size*0.8)
    if id =='tc2-05':
        plt.ylim(-2e5,5e6)
    else:
        plt.ylim(-1e5,5e6)
    plt.yticks(fontsize=font_size*0.8)

    plt.plot(time2,predical_delta_Q2,label='predical_delta_Q',linewidth=linewidth,marker='^',color='orange')
    plt.plot(time2,Sr2,label='Sr',linewidth=linewidth,marker='o',color='r')

    ax=plt.gca()

    ax2=ax.twinx()
    ax2.set_ylabel('lambda and Dr',fontsize=font_size*0.8)
    ax2.set_ylim(-100,1200)
    ax2.tick_params(labelsize=font_size*0.8,direction='in')

    ax2.yaxis.set_major_locator(MultipleLocator(200))
    ax2.plot(time2,Dr2,label='Dr',linewidth=linewidth,marker='*',color='g')
    ax2.plot(time2,predical_lambda2,label='predical_lambda',linewidth=linewidth,marker='p',color='k')

    lines1, labels1 = ax.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    plt.legend(lines1+lines2, labels1+labels2,loc=(0,1.05),ncol=2)

    legend =plt.gca().get_legend()
    legend.get_frame().set_linewidth(0)
    plt.subplots_adjust(top=0.8,bottom=0.1,left=0.1,right=0.85) #调整子图相对于图形边缘的位置

    save=save_path+'pbs'+'/'+id+'/'+iftest8SendRate+'/'+'Dr_Sr_Q-port2'
    plt.savefig(save)
    plt.clf()




### main part.
# if __name__=='__main__':
if not os.path.exists(save_path):
    os.makedirs(save_path)

if len(sys.argv) <= 2:
    print("请传入测试用例编号和缓存算法，example:tc2-04 BMS")
else:
    testcase_number = sys.argv[1] #测试用例编号
    print(f'testcase_number={testcase_number}')
    testcase_name = sys.argv[2] #缓存算法
    print(f'testcase_name={testcase_name}')
    if testcase_number == 'tc2-08':
        test8_plot(testcase_number,testcase_name)
        for rate in ["100","200","300","400","500","600","700","800","900"]:
            # cost_etc_details_plot(testcase_number,testcase_name,rate)
            # lambda_cost_etc_details_plot(testcase_number,testcase_name,rate)
            # T_AI_MD_details(testcase_number,testcase_name,rate)
            # lambda_Dr_details_plot(testcase_number,rate)
            # delta_Q_detail(testcase_number,rate)
            # lambda_C_i_sout(testcase_number,rate)
            # Dr_Sr_Q_plot(testcase_number,rate)
            # queue_write_read_throughput_plot(testcase_number,testcase_name,rate)
            queue_usage_plot(testcase_number,testcase_name,rate)
    elif testcase_number == 'tc2-08-new':
        # 新场景8
        test8_new_plot( testcase_number, testcase_name )
        for change_time_dir in ["2us","4us","8us","16us","32us","64us"]:
            queue_usage_plot(testcase_number,testcase_name,change_time_dir)

    elif testcase_number == 'tc2-09':
        test9_plot()
    else:
        queue_write_read_throughput_plot(testcase_number,testcase_name)
        queue_usage_plot(testcase_number,testcase_name)
        
        cost_etc_details_plot(testcase_number,testcase_name)
        lambda_cost_etc_details_plot(testcase_number,testcase_name)
        T_AI_MD_details(testcase_number,testcase_name)
        #U2_U2star_details_plot(testcase_number)
        lambda_Dr_details_plot(testcase_number)
        #U2_U2star_detail(testcase_number)
        delta_Q_detail(testcase_number)
        lambda_C_i_sout(testcase_number)
        Dr_Sr_Q_plot(testcase_number)





