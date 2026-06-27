#coding:utf-8
import os
import numpy as np
import re
import sys
import traceback
import random
import matplotlib.pyplot as plt
from matplotlib.pyplot import MultipleLocator 


# plt.rcParams['font.sans-serif'] = ['SimSun'] # 用来正常显示中文标签SimHei
# plt.rcParams['axes.unicode_minus'] = False # 用来正常显示负号

plt.rcParams['legend.fontsize'] = 15

plt.rc('font',family='Times New Roman')
#服务器pba
data_dir =f'/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/pbs/' #数据所在目录
save_path = f'/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data-fig/' #存储路径

#服务器BMS
data_dir_BMS =f'/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/BMS/' #数据所在目录
save_path_BMS = f'/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data-fig/' #存储路径

xzx_line_width = 3
# xzx_legend_size = 26
xzx_legend_size = 26
xzx_xykedu_size = 20

nPort = 64
nPrio = 2
nQueue = 5

interval = 100
KB = 1024
MB = KB*1024
GB = MB*1024
Gbps = 1000*1000*1000
ns = 1000*1000*1000
Value=1000;

xy_valaue=20;
x_valaue=1;
linewidth=1

xy_label_fontsize = 20

x_major_locator=MultipleLocator(1)
y_major_locator=MultipleLocator(500)
# x_major_locator=MultipleLocator(0.005)

x_start = 0.0
x_end = 0.8

# DRAM/SRAM缓存使用量
def buffer_usage_plot(id):
    
    # pbs filename
    filename = data_dir + id + '/' + "buffer-usage-test-"+testcase_number+".csv"
    # BMS filename
    filename_BMS = data_dir_BMS + id + '/' +"buffer-usage-test-"+testcase_number+".csv"

    
    with open(filename) as file_pbs,open(filename_BMS) as file_BMS:
        next(file_pbs)
        next(file_BMS)

        time_pba = []
        sram_pba = []
        wcache_pba = []
        dram_pba = []
        lineNum_pba = 0

        time_BMS = []
        sram_BMS = []
        wcache_BMS = []
        dram_BMS = []
        lineNum_BMS = 0
        # pbs
        for line_pba in file_pbs:
            lineNum_pba += 1
            if lineNum_pba % interval == 0 or lineNum_pba // interval < 2:
                line_pba = line_pba.split(',')
                if len(line_pba) < 4:
                    continue
                time_pba.append(float(line_pba[0])*Value)
                sram_pba.append(float(line_pba[1])/MB)
                wcache_pba.append(float(line_pba[2])/KB)
                dram_pba.append(float(line_pba[3][:-1])/MB)    # remove \n at the end of line
        # BMS
        for line_BMS in file_BMS:
            lineNum_BMS += 1
            if lineNum_BMS % interval == 0 or lineNum_BMS // interval < 2:
                line_BMS = line_BMS.split(',')
                if len(line_BMS) < 4:
                    continue
                time_BMS.append(float(line_BMS[0])*Value)
                sram_BMS.append(float(line_BMS[1])/MB)
                wcache_BMS.append(float(line_BMS[2])/KB)
                dram_BMS.append(float(line_BMS[3][:-1])/MB)    # remove \n at the end of line

        
        # time_hw=[x*1000 for x in time_hw]
        # time_pba=[x*1000 for x in time_pba]

        if id == "tc2-01":
            # plt.xlim(x_start,x_end*1.25)
            plt.xticks(fontsize=xy_valaue)  # 设置横坐标刻度字体大小为10
        elif id == "tc2-02":
            # plt.xlim(x_start,x_end*0.75)
            plt.xticks(fontsize=xy_valaue)
        elif id == "tc2-03":
            # plt.xlim(x_start,x_end*1.75)
            plt.xticks(fontsize=xy_valaue)
        elif id == "tc2-04" :
            # plt.xlim(x_start,x_end*4)
            plt.xticks(fontsize=xy_valaue*0.5)
        else :
            # plt.xlim(x_start,x_end*2.5)
            plt.xticks(fontsize=xy_valaue)

        # plt.ylim(-0.2,2)
        # plt.xlim(0,2) #wk
        plt.xlabel('Time(ms)',fontsize=xy_label_fontsize)
        plt.ylabel('SRAM-buffer-usage(MB)',fontsize=xy_label_fontsize)
        plt.yticks(fontsize=xy_valaue)  # 设置纵坐标刻度字体大小为10
        plt.tick_params(axis='x', direction='in')
        plt.tick_params(axis='y', direction='in')
        
        ax=plt.gca()
        #ax.xaxis.set_major_locator(MultipleLocator(0.2))
        ax.yaxis.set_major_locator(MultipleLocator(1))

        plt.plot(time_pba, sram_pba, label='FBM',linewidth=linewidth,marker='o',linestyle='-',color='r')
        plt.plot(time_BMS, sram_BMS, label='DeepHir',linewidth=linewidth,marker='^',linestyle='-',color='k')
        plt.tight_layout()
        plt.legend(frameon=False).set_visible(True)
        path = save_path + 'compare/'+id+'/buffer-usage-'  + '-SRAM-BMS-vs-pbs' + '.png'
        plt.savefig(path)
        # plt.show()
        plt.clf()
        
        if id == "tc2-01":
            # plt.xlim(x_start,x_end*1.25)
            plt.xticks(fontsize=xy_valaue)  # 设置横坐标刻度字体大小为10
        elif id == "tc2-02":
            # plt.xlim(x_start,x_end*0.75)
            plt.xticks(fontsize=xy_valaue)
        elif id == "tc2-03":
            # plt.xlim(x_start,x_end*1.75)
            plt.xticks(fontsize=xy_valaue)
        elif id == "tc2-04" :
            # plt.xlim(x_start,x_end*4)
            plt.xticks(fontsize=xy_valaue*0.5)
        else :
            # plt.xlim(x_start,x_end*2.5)
            plt.xticks(fontsize=xy_valaue)

        plt.ylim(-0.2,8)
        plt.xlim(0,2) #wk
        plt.xlabel('Time(ms)',fontsize=xy_label_fontsize)
        plt.ylabel('DRAM-buffer-usage(MB)',fontsize=xy_label_fontsize)
        plt.yticks(fontsize=xy_valaue)  # 设置纵坐标刻度字体大小为10
        plt.tick_params(axis='x', direction='in')
        plt.tick_params(axis='y', direction='in')

        ax=plt.gca()
        #ax.xaxis.set_major_locator(MultipleLocator(0.2))
        ax.yaxis.set_major_locator(MultipleLocator(1))
        
        plt.plot(time_pba, dram_pba, label='FBM',linewidth=linewidth,marker='o',linestyle='-',color='r')
        plt.plot(time_BMS, dram_BMS, label='DeepHir',linewidth=linewidth,marker='^',linestyle='-',color='k')
        plt.tight_layout()
        plt.legend(frameon=False).set_visible(True)
        plt.legend(loc='upper right')
        path = save_path + 'compare/'+id+'/buffer-usage-' + '-DRAM-BMS-vs-pbs' + '.png'
        plt.savefig(path)
        # plt.show()
        plt.clf()



def get_loss_delay(id):
    filename_pbs = data_dir + id+ '/hybrid-buffer-test-'+id+'.txt'
    filename_bms = data_dir_BMS + id+ '/hybrid-buffer-test-'+id+'.txt'
    
    with open(filename_pbs) as pbs_txt:
        lines = pbs_txt.readlines()
        i = len(lines)-1

        loss_pbs = 0
        delay_pbs = 0
        loss_flag = 0 #标记loss_pbs是否已经记录
        delay_flag = 0


        while (loss_flag == 0 or delay_flag == 0) and i >= 0:
            line = lines[i].split()
            if "当前丢包率：" in line and loss_flag == 0:
                j = line.index("当前丢包率：")
                s = line[j+1].index("%")
                loss_pbs = float(line[j+1][:s])
                loss_flag = 1

            elif "m_avgDelay(ms):" in line and delay_flag == 0:
                j = line.index("m_avgDelay(ms):")
                delay_pbs = float(line[j+1])
                delay_flag = 1

            i -= 1
        

    with open(filename_bms) as bms_txt:
        lines = bms_txt.readlines()
        i = len(lines)-1

        loss_bms = 0
        delay_bms = 0
        loss_flag = 0 #标记loss_bms是否已经记录
        delay_flag = 0

        while (loss_flag == 0 or delay_flag == 0) and i >= 0:
            line = lines[i].split()
            if "当前丢包率：" in line and loss_flag == 0:
                j = line.index("当前丢包率：")
                s = line[j+1].index("%")
                loss_bms = float(line[j+1][:s])
                loss_flag = 1

            elif "m_avgDelay:" in line and delay_flag == 0:
                j = line.index("m_avgDelay:")
                delay_bms = float(line[j+1])
                delay_flag = 1

            i -= 1

    file_vs = save_path + 'compare/'+id+'/BMS-vs-pbs.txt'
    with open(file_vs, "w") as vs_txt:
        vs_txt.write("pbs下: 丢包率: "+ str(loss_pbs)+ "%      m_avgDelay(ms): "+str(delay_pbs)+"\n")
        vs_txt.write("BMS下: 丢包率: "+ str(loss_bms)+ "%      m_avgDelay: "+str(delay_bms)+"\n")




# 片外吞吐
def hbm_throughput_plot(id):

    # pbs filename
    filename = data_dir +  id + '/' + "hbm-throughput-test-"+testcase_number+".csv"
    # BMS filename
    filename_BMS = data_dir_BMS + id + '/' + "hbm-throughput-test-"+testcase_number+".csv"
    with open(filename) as file_pbs,open(filename_BMS) as file_BMS:
        lines_pba = file_pbs.readlines()
        lines_BMS = file_BMS.readlines()
        if len(lines_pba) <= 1:
            return
        if len(lines_BMS) <= 1:
            return

        time_pba = []
        read_pba = []
        write_pba = []
        line1_pba = lines_pba[1].split(',')
        start_pba = float(line1_pba[0])
        inter_pba = float(line1_pba[1]) - start_pba

        time_BMS = []
        read_BMS = []
        write_BMS = []
        line1_BMS = lines_BMS[1].split(',')
        start_BMS = float(line1_BMS[0])
        inter_BMS = float(line1_BMS[1]) - start_BMS

        time_pba.append(start_pba*Value)
        read_pba.append(0)
        write_pba.append(0)

        time_BMS.append(start_BMS*Value)
        read_BMS.append(0)
        write_BMS.append(0)

        for line_pba in lines_pba[1:]:
            if len(line_pba) < 4:
                continue
            line_pba = line_pba.split(',')
            cur_pba = float(line_pba[1])
            time_pba.append(cur_pba*Value)
            read_pba.append(float(line_pba[2])/Gbps)
            write_pba.append(float(line_pba[3][:-1])/Gbps)
        
        for line_BMS in lines_BMS[1:]:
            if len(line_BMS) < 4:
                continue
            line_BMS = line_BMS.split(',')
            cur_BMS = float(line_BMS[1])
            time_BMS.append(cur_BMS*Value)
            read_BMS.append(float(line_BMS[2])/Gbps)
            write_BMS.append(float(line_BMS[3][:-1])/Gbps)
    
    loss_packet =data_dir + id +"/loss_packet.csv"
    loss_packet_BMS =data_dir_BMS + id +"/loss_packet.csv"
    
    with open(loss_packet) as file,open(loss_packet_BMS) as file_BMS:
        lines = file.readlines()
        lines_BMS = file_BMS.readlines()
        if len(lines) <= 1:
            return
        if len(lines_BMS) <= 1:
            return
        time1 = []
        loss1 = []
        line1 = lines[0].split(',')
        start = float(line1[0])
        inter = float(line1[1]) - start
        time1.append(start*Value)
        loss1.append(0)

        time1_BMS = []
        loss1_BMS = []
        line1_BMS = lines_BMS[0].split(',')
        start_BMS = float(line1_BMS[0])
        inter_BMS = float(line1_BMS[1]) - start_BMS
        time1_BMS.append(start_BMS*Value)
        loss1_BMS.append(0)

        for line in lines:
            if len(line) < 4:
                continue
            line = line.split(',')
            try:
                cur = float(line[1])
            except ValueError:
                print("出现文字转浮点数失败,自动跳过,详细错误如下：")
                print(traceback.format_exc())
                continue
            time1.append(cur*Value)
            loss1.append(float(line[2]))
        
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
            time1_BMS.append(cur*Value)
            loss1_BMS.append(float(line_BMS[2]))
            

        plt.ylim(-30,1200)
        if id == "tc2-01":
            # plt.xlim(x_start,x_end*1.25)
            plt.xticks(fontsize=xy_valaue)  # 设置横坐标刻度字体大小为10
        elif id == "tc2-02":
            # plt.xlim(x_start,x_end*0.75)
            plt.xticks(fontsize=xy_valaue)
        elif id == "tc2-03":
            # plt.xlim(x_start,x_end*1.75)
            plt.xticks(fontsize=xy_valaue)
        elif id == "tc2-04" :
            # plt.xlim(x_start,x_end*4)
            plt.xticks(fontsize=xy_valaue*0.5)
        else :
            # plt.xlim(x_start,x_end*2.5)
            plt.xticks(fontsize=xy_valaue)
        
        plt.xlim(0,2) #wk
        plt.xlabel('Time(ms)',fontsize=xy_label_fontsize)
        plt.ylabel('DRAM-buffer-throughput(Gbps)',fontsize=xy_label_fontsize)
        plt.yticks(fontsize=xy_valaue)  # 设置纵坐标刻度字体大小为10
        plt.tick_params(axis='x', direction='in')
        plt.tick_params(axis='y', direction='in')
        

        ax=plt.gca()
        #ax.xaxis.set_major_locator(MultipleLocator(0.2)) #设置x轴坐标间隔
        ax.yaxis.set_major_locator(MultipleLocator(200)) #设置y轴坐标间隔
        
        plt.plot(time_pba, read_pba, label='read_FBM',linewidth=linewidth,marker='^',linestyle='--',color='lightcoral',markersize=1)
        plt.plot(time_pba, write_pba, label='write_FBM',linewidth=linewidth,marker='s',linestyle='-',color='r',markersize=1)

        plt.plot(time_BMS, read_BMS, label='read_DeepHir',linewidth=linewidth,marker='+',linestyle='--',color='dimgray',markersize=1)
        plt.plot(time_BMS, write_BMS, label='write_DeepHir',linewidth=linewidth,marker='o',linestyle='-',color='black',markersize=1)

        plt.tight_layout()
        # plt.legend().remove()
        # plt.legend(loc='upper left')
        plt.legend(frameon=False).set_visible(True)
        plt.legend(loc="upper right")
        path = save_path+ '/compare'+'/'+id+ '/hbm-throughput-'  + '-DRAM-BMS-vs-pbs' + '.png'
        plt.savefig(path)
        # plt.show()
        plt.clf()


        #画loss的图

        if id == "tc2-01":
            plt.ylim(-20,600)
            # plt.xlim(x_start,x_end*1.25)
            plt.xticks(fontsize=xy_valaue)  # 设置横坐标刻度字体大小为10
        elif id == "tc2-02":
            plt.ylim(-20,800)
            # plt.xlim(x_start,x_end*0.75)
            plt.xticks(fontsize=xy_valaue)
        elif id == "tc2-03":
            plt.ylim(-20,600)
            # plt.xlim(x_start,x_end*1.75)
            plt.xticks(fontsize=xy_valaue)
        elif id == "tc2-04" :
            plt.ylim(-20,600)
            # plt.xlim(x_start,x_end*4)
            plt.xticks(fontsize=xy_valaue*0.5)
        else :
            plt.ylim(-20,600)
            # plt.xlim(x_start,x_end*2.5)
            plt.xticks(fontsize=xy_valaue)

        plt.xlabel('Time(ms)',fontsize=xy_label_fontsize)
        plt.ylabel('DRAM-buffer-throughput(Gbps)',fontsize=xy_label_fontsize)
        plt.yticks(fontsize=xy_valaue)  # 设置纵坐标刻度字体大小为10
        plt.tick_params(axis='x', direction='in')
        plt.tick_params(axis='y', direction='in')

        plt.plot(time1, loss1, label='loss_FBM',linewidth=linewidth,marker='d',linestyle='-',color='r',markersize=1)
        plt.plot(time1_BMS, loss1_BMS, label='loss_DeepHir',linewidth=linewidth,marker='*',linestyle='-',color='k',markersize=1)

        ax=plt.gca()
        #ax.xaxis.set_major_locator(MultipleLocator(0.2)) #设置x轴坐标间隔
        ax.yaxis.set_major_locator(MultipleLocator(200)) #设置y轴坐标间隔
        
        plt.tight_layout()
        plt.legend(frameon=False).set_visible(True)
        path = save_path+ '/compare'+'/'+ id +'/hbm-throughput-'  + '-DRAM-BMS-vs-pbs-loss' + '.png'
        plt.savefig(path)
        plt.clf()

# 片内吞吐
def sram_throughput_plot(id):
    # pbs filename
    filename = data_dir + id + '/' + "sram-throughput-test-"+testcase_number+".csv"
    # BMS filename
    filename_BMS = data_dir_BMS  +id + '/' + "sram-throughput-test-"+testcase_number+".csv"
    
    with open(filename) as file_pbs,open(filename_BMS) as file_BMS:
        lines_pba = file_pbs.readlines()
        lines_BMS = file_BMS.readlines()
        
        if len(lines_pba) <= 1:
            return
        if len(lines_BMS) <= 1:
            return

        time_pba = []
        read_pba = []
        write_pba = []
        line1_pba = lines_pba[1].split(',')
        start_pba = float(line1_pba[0])
        inter_pba = float(line1_pba[1]) - start_pba
        time_pba.append(start_pba*Value)
        read_pba.append(0)
        write_pba.append(0)

        time_BMS = []
        read_BMS = []
        write_BMS = []
        line1_BMS = lines_BMS[1].split(',')
        start_BMS = float(line1_BMS[0])
        inter_BMS = float(line1_BMS[1]) - start_BMS
        time_BMS.append(start_BMS*Value)
        read_BMS.append(0)
        write_BMS.append(0)

        for line_pba in lines_pba[1:]:
            if len(line_pba) < 4:
                continue
            line_pba = line_pba.split(',')
            cur_pba = float(line_pba[1])
            time_pba.append(cur_pba*Value)
            read_pba.append(float(line_pba[2])/Gbps)
            write_pba.append(float(line_pba[3][:-1])/Gbps)
        
        for line_BMS in lines_BMS[1:]:
            if len(line_BMS) < 4:
                continue
            line_BMS = line_BMS.split(',')
            cur_BMS = float(line_BMS[1])
            time_BMS.append(cur_BMS*Value)
            read_BMS.append(float(line_BMS[2])/Gbps)
            write_BMS.append(float(line_BMS[3][:-1])/Gbps)

        if id == "tc2-01":
            # plt.xlim(x_start,x_end*1.25)
            plt.xticks(fontsize=xy_valaue)  # 设置横坐标刻度字体大小为10
        elif id == "tc2-02":
            # plt.xlim(x_start,x_end*0.75)
            plt.xticks(fontsize=xy_valaue)
        elif id == "tc2-03":
            # plt.xlim(x_start,x_end*1.75)
            plt.xticks(fontsize=xy_valaue)
        elif id == "tc2-04" :
            # plt.xlim(x_start,x_end*4)
            plt.xticks(fontsize=xy_valaue*0.5)
        else :
            # plt.xlim(x_start,x_end*2.5)
            plt.xticks(fontsize=xy_valaue)
        plt.xlim(0,2) #wk
        plt.ylim(-20,1500)
        plt.xlabel('Time(ms)',fontsize=xy_label_fontsize)
        plt.ylabel('SRAM-buffer-throughput(Gbps)',fontsize=xy_label_fontsize)
        plt.yticks(fontsize=xy_valaue)  # 设置纵坐标刻度字体大小为10
        plt.tick_params(axis='x', direction='in')
        plt.tick_params(axis='y', direction='in')
        
        ax=plt.gca()
        #ax.xaxis.set_major_locator(MultipleLocator(0.2))
        # ax.yaxis.set_major_locator(y_major_locator)
        plt.plot(time_pba, read_pba, label='read_FBM',linewidth=linewidth,marker='^',linestyle='--',color='lightcoral',markersize=1)
        plt.plot(time_pba, write_pba, label='write_FBM',linewidth=linewidth,marker='s',linestyle='-',color='r',markersize=1)

        plt.plot(time_BMS, read_BMS, label='read_DeepHir',linewidth=linewidth,marker='+',linestyle='--',color='dimgray',markersize=1)
        plt.plot(time_BMS, write_BMS, label='write_DeepHir',linewidth=linewidth,marker='*',linestyle='-',color='k',markersize=1)
        plt.tight_layout()
        # plt.legend(loc='upper left')
        # plt.legend().set_visible(True)
        plt.legend(frameon=False,ncol=2).set_visible(True)
        plt.legend(loc='upper right')

        path = save_path  + '/compare/'+ id +'/sram-throughput-' + '-SRAM-BMS-vs-pbs' + '.png'
        plt.savefig(path)
        # plt.show()
        plt.clf()

def buffer_loss_compare(id):
    # pbs filename
    filename = data_dir + id + '/' + "buffer-usage-test-"+testcase_number+".csv"
    # BMS filename
    filename_BMS = data_dir_BMS + id + '/' +"buffer-usage-test-"+testcase_number+".csv"
    loss_packet =data_dir + id +"/loss_packet.csv"
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
        
        flag=1
        interval=50
        flag0=0

        for line in lines:
            if len(line) < 4:
                continue
            line = line.split(',')
            try:
                cur = float(line[1])
            except ValueError:
                print("出现文字转浮点数失败,自动跳过,详细错误如下：")
                print(traceback.format_exc())
                continue
            flag0+=1
            if flag==1 and float(line[2])==0:
                if flag0 % interval !=0:
                    continue
                loss_pbs.append(float(line[5]))
                time_loss_pbs.append(cur*Value)
                continue
            flag=0
            if len(line) == 6:
                if len(loss_pbs)>1:
                    loss_pbs.append(float(line[5]))
                    time_loss_pbs.append(cur*Value)

        flag=1
        for line_BMS in lines_BMS:
            if len(line_BMS) < 4:
                continue
            line_BMS = line_BMS.split(',')
            try:
                # lossOfBMS=float(line_BMS[-1])*1500/1024#之前是float(line_BMS[5])/1000，改成float(line_BMS[5])*1500/1024
                lossOfBMS=float(line_BMS[-1])
                cur = float(line_BMS[1])
            except ValueError:
                print("出现文字转浮点数失败,自动跳过,详细错误如下：")
                print(traceback.format_exc())
                continue
            if flag==1 and float(line_BMS[2])==0:
                loss_BMS.append(lossOfBMS)
                time_loss_BMS.append(cur*Value)
                continue
            flag=0
            if len(line_BMS) == 6:
                if len(loss_BMS)>1:
                    loss_BMS.append(lossOfBMS)#lossOfBMS+loss_BMS[-1]
                    time_loss_BMS.append(cur*Value)
                else:
                    loss_BMS.append(lossOfBMS)
                    time_loss_BMS.append(cur*Value)


    if id == "tc2-06" :
        plt.ylim(0,4)

    plt.xticks(fontsize=xzx_legend_size)
    plt.xlabel('Time(ms)',fontsize=xzx_legend_size)

    
    plt.yticks(fontsize=xzx_legend_size)
    plt.ylabel('Buffer Usage (MB)',fontsize=xzx_legend_size)
    plt.tick_params(axis='x',direction='in')
    plt.tick_params(axis='y',direction='in')

    ax=plt.gca()


    if id=="tc2-05":
        plt.ylim(0,5)
        plt.xlim(0,2) #wk
        ax.xaxis.set_major_locator(MultipleLocator(0.4))
        ax.yaxis.set_major_locator(MultipleLocator(1))
       
    elif id=="tc2-06":
        plt.xlim(-0.2,5.8)
        plt.ylim(-0.2,3.8)      
        ax.xaxis.set_major_locator(MultipleLocator(1))
        ax.yaxis.set_major_locator(MultipleLocator(1))
    elif id=='tc2-07':
        plt.ylim(-0.2,3.8)
        plt.xlim(-0.2,8)   
        ax.yaxis.set_major_locator(MultipleLocator(1))

    lines = [0,0]
    if id=="tc2-06" or id == "tc2-07":
        num1 = int(len(time_BMS) * 0.3)  # 计算要保留的数据个数，向下取整
        num2 = int(len(time_pbs) * 0.3)  # 计算要保留的数据个数，向下取整
        lines[0], =plt.plot(time_BMS[:num1], sram_BMS[:num1], label='DeepHir',linewidth=xzx_line_width,linestyle='--',color='k')
        lines[1], =plt.plot(time_pbs[:num2], sram_pbs[:num2], label='FBM',linewidth=xzx_line_width,linestyle='-',color='r')
    else:
        lines[0], =plt.plot(time_BMS, sram_BMS, label='DeepHir',linewidth=xzx_line_width,linestyle='--',color='k')
        lines[1], =plt.plot(time_pbs, sram_pbs, label='FBM',linewidth=xzx_line_width,linestyle='-',color='r')
    handles,labels = ax.get_legend_handles_labels()


    plt.tight_layout()
    plt.legend(frameon=False).set_visible(True)
    plt.subplots_adjust(left=0.16, right=0.98, top=0.98,bottom=0.19) #调整子图相对于图形边缘的位置
    # plt.subplots_adjust(top=0.85,left=0.19,right=0.86)
    # plt.subplots_adjust(left=0.15)


    if id =="tc2-05":
        plt.legend(loc=2, borderaxespad=0,handles=lines,labels=["DeepHir","FBM"],ncol=2,fontsize=xzx_legend_size,frameon=False)
    elif id=="tc2-06":
        plt.legend(loc=2, handletextpad=0.2, columnspacing=1.5,bbox_to_anchor=(0.1, 1),borderaxespad=0,handles=lines,labels=["DeepHir","FBM"],ncol=2,fontsize=xzx_legend_size,frameon=False)
    elif id=="tc2-07":
        plt.legend(loc=2, handletextpad=0.2, columnspacing=1.5,bbox_to_anchor=(0.1, 1),borderaxespad=0,handles=lines,labels=["DeepHir","FBM"],ncol=2,fontsize=xzx_legend_size,frameon=False)

    legend =plt.gca().get_legend()
    # legend.get_frame().set_linewidth(0)

    path = save_path + 'compare/'+id + '/buffer-BMS-vs-pbs' + '.pdf'
    plt.savefig(path)

    plt.clf()


    plt.figure(figsize=(8,6)) 
    plt.subplots_adjust(left=0.155, right=0.98, top=0.98,bottom=0.17) #调整子图相对于图形边缘的位置
    plt.xticks(fontsize=xzx_legend_size)
    plt.xlabel('Time(ms)',fontsize=xzx_legend_size)
    if id == "tc2-06":
        plt.ylabel('# of Packet Loss (x1e3)',fontsize=xzx_legend_size)
    elif id == "tc2-07":
        plt.ylabel('# of Packet Loss (x1e3)',fontsize=xzx_legend_size)
        plt.ylim(0,62)
    else:
        plt.ylabel('# of Packet Loss',fontsize=xzx_legend_size)
    plt.yticks(fontsize=xzx_legend_size)
    plt.tick_params(axis='x',direction='in')
    plt.tick_params(axis='y',direction='in')

    

    ax=plt.gca()

    if id=="tc2-05":
        ax.xaxis.set_major_locator(MultipleLocator(0.4))
    elif id=="tc2-06":
        ax.xaxis.set_major_locator(MultipleLocator(4))
    elif id=="tc2-07":
        ax.xaxis.set_major_locator(MultipleLocator(5))

    line_rate=2
    size=6

    xzx_line_width5 =4

    if id =="tc2-06":
        plt.plot(time_loss_BMS,[num /1e3 for num in loss_BMS],label='DeepHir',linewidth=xzx_line_width,linestyle='--',color='k',markersize=size)
        plt.plot(time_loss_pbs,[num /1e3 for num in loss_pbs],label='FBM',linewidth=xzx_line_width,linestyle='-',color='r',markersize=size)
    elif id =="tc2-07":
        plt.plot(time_loss_BMS,[num /1e3 for num in loss_BMS],label='DeepHir',linewidth=xzx_line_width,linestyle='--',color='k',markersize=size)
        plt.plot(time_loss_pbs,[num /1e3 for num in loss_pbs],label='FBM',linewidth=xzx_line_width,linestyle='-',color='r',markersize=size)
    else:
        plt.plot(time_loss_BMS,loss_BMS,label='DeepHir',linewidth=xzx_line_width5,linestyle='--',marker='o',color='k',markersize=size)
        plt.plot(time_loss_pbs,loss_pbs,label='FBM',linewidth=xzx_line_width5,linestyle='-',marker='d',color='r',markersize=size)


    if id=="tc2-05":
        fontsize205 = xzx_legend_size+2
        plt.xticks(fontsize=fontsize205)
        plt.yticks(fontsize=fontsize205)
        plt.ylabel('# of Packet Loss',fontsize=fontsize205)
        plt.legend(loc=2,labels=["DeepHir","FBM"],ncol=1,fontsize=fontsize205,frameon=False)
        
    elif id=="tc2-06":
        plt.legend(loc=2, bbox_to_anchor=(0, 0.99),labels=["DeepHir","FBM"],ncol=1,fontsize=xzx_legend_size,frameon=False)
    elif id=="tc2-07":
        plt.legend(loc=2, bbox_to_anchor=(0, 0.99),labels=["DeepHir","FBM"],ncol=1,fontsize=xzx_legend_size,frameon=False)

    legend =plt.gca().get_legend()
    
    # legend.get_frame().set_linewidth(0)
    
    path = save_path + 'compare/'+id + '/loss-BMS-vs-pbs' + '.pdf'
    plt.savefig(path)
    plt.clf()


    


def total_sram_usage(id):
    filename = data_dir + id +'/'+'queue-sram-usage-test-'+id+'.csv'
    filename_BMS = data_dir_BMS+id+'/'+'queue-sram-usage-test-'+id+'.csv'
    with open(filename) as file,open(filename_BMS) as file_BMS:
        time = []
        sram = []
        time2 = []
        sram2 = []

        lines = file.readlines()
        line = lines[1].split(',')
        start = float(line[0]) #起始时间
        interval = 1e-6 #时间周期
        last_time = 0
        total = 0 
        total_num = 0 

        while(last_time+interval<start):
            time.append(last_time)
            sram.append(0)
            last_time+=interval

        end_flag = 0

        for line in lines[1:]:
            if lines[-1]==line:
                end_flag=1
            line = line.split(',')
            if len(line) < 5:
                continue
            time0 = float(line[0])
            sram0 = float(line[4])/MB

            n = int((time0 - last_time)/interval)
            if n <= 1 and end_flag==0: #time还落在一个间隔内，继续加
                total+=sram0
                total_num+=1
            else:
                if total_num!=0:
                    time.append(last_time*Value)
                    sram.append(total/total_num)
                    
                    last_time+=interval
                n = int((time0 - last_time)/interval)
                for i in range(0,n):
                    time.append(last_time*Value)
                    sram.append(sram[len(sram)-1])
                    last_time+=interval
                total=sram0
                total_num=1

        
        lines = file_BMS.readlines()

        line = lines[1].split(',')
        start = float(line[0])
        last_time = 0
        total = 0
        total_num = 0

        while(last_time+interval<start):
            time.append(last_time)
            sram.append(0)
            last_time+=interval

        end_flag=0

        for line in lines[1:]:
            if lines[-1]==line:
                end_flag=1
            line = line.split(',')
            if len(line) < 5:
                continue
            time0 = float(line[0])
            sram0 = float(line[4])/MB
            
            n = int((time0 - last_time)/interval)
            if n <= 1 and end_flag==0: #time还落在一个间隔内，继续加
                total+=sram0
                total_num+=1
            else:
                if total_num!=0:
                    time2.append(last_time*Value)
                    sram2.append(total/total_num)
                    
                    last_time+=interval
                n = int((time0 - last_time)/interval)
                
                for i in range(0,n):
                    time2.append(last_time*Value)
                    sram2.append(sram2[len(sram2)-1])
                    last_time+=interval
                total=sram0
                total_num=1

    
        plt.ylim(-0.2,4)
        plt.xlim(-0.1,1.2)
        plt.tick_params(axis='x',direction = 'in')
        plt.xticks(fontsize = xy_valaue*0.8)
        plt.xlabel('Time(ms)',fontsize=25)

        plt.tick_params(axis='y',direction = 'in')
        plt.yticks(fontsize = xy_valaue*0.8)
        plt.ylabel('Total SRAM Usage(MB)',fontsize=25)

        ax=plt.gca()
        #ax.xaxis.set_major_locator(MultipleLocator(0.2))
        ax.yaxis.set_major_locator(MultipleLocator(1))
        
        plt.plot(time,sram,label='FBM',linewidth=linewidth,color='k')
        plt.plot(time2,sram2,label='DeepHir',linewidth=linewidth,color='r')
        plt.legend(frameon=False)
        legend = ax.get_legend()
        legend.get_frame().set_linewidth(0)

        path = save_path + '/compare'+'/'+id+'/'+'total-sram-usage--BMS-vs-pbs'+'.png'
        plt.savefig(path)
        plt.clf()


def Dram_total_throuphput(id):

    time1 = [[],[]] #列表中存pbs和BMS的时间列表
    write = [[],[]]
    time2 = [[],[]]
    read = [[],[]]
    time3 = [[],[]]
    loss = [[],[]]
    interval = 5e-5

    
    filename_loss = data_dir+id+'/'+'loss_packet.csv'
    filename_loss_BMS = data_dir_BMS+id+'/'+'loss_packet.csv'

    
    for i in range(0,2):#i=0是pbs，i=1是BMS
        for port in range(0,3):
            filename_write = data_dir+id+'/'+'queue-hbm-write-throughput-test-'+id+'-p'+str(port)+'.csv'
            filename_read = data_dir+id+'/'+'queue-hbm-read-throughput-test-'+id+'-p'+str(port)+'.csv'
            filename_write_BMS = data_dir_BMS+id+'/'+'queue-hbm-write-throughput-test-'+id+'-p'+str(port)+'.csv'
            filename_read = data_dir_BMS+id+'/'+'queue-hbm-read-throughput-test-'+id+'-p'+str(port)+'.csv'
            
            file_write=[filename_write,filename_write_BMS]
            file_read=[filename_read,filename_read]

            with open(file_write[i]) as file_write,open(file_read[i]) as file_read:
                
                #写端口
                lines = file_write.readlines()
                if(len(lines)>1):
                    last_time = 0
                    line = lines[1].split(',')

                    start= float(line[0])
                    total = 0
                    total_num = 0
                    num = 0 #time的append的次数
                    if(port==0):
                        while(last_time+interval<start):
                            time1[i].append(last_time*Value)
                            write[i].append(0)
                            last_time+=interval
                            num+=1
                    else:
                        while(last_time+interval<start):
                            last_time+=interval
                            num+=1

                    end_flag = 0 #标记文件是否到了最后一行

                    for line in lines[1:]:
                        if lines[-1]==line:
                            end_flag=1
                        line = line.split(',')
                        if len(line) <5:
                            continue
                        time = float(line[0])
                        rate = float(line[4])/Gbps
                        
                        if time <= last_time+interval and end_flag==0:
                            total+=rate
                            total_num+=1
                        else:
                            if total_num!=0:
                                if len(time1[i]) <= num:
                                    time1[i].append(last_time*Value)
                                    write[i].append(total/total_num)
                                else:
                                    write[i][num]+=total/total_num
                                num+=1
                                last_time+=interval
                            n = int((time - last_time)/interval)
                            if n>=4:
                                for j in range(0,n):
                                    if len(time1[i]) <= num:
                                        time1[i].append(last_time*Value)
                                        write[i].append(0)
                                    num+=1
                                    last_time+=interval
                            else:
                                for j in range(0,n):
                                    if len(time1[i]) <= num:
                                        time1[i].append(last_time*Value)
                                        write[i].append(write[i][len(write[i])-1])
                                    else:
                                        write[i][num]+=write[i][len(write[i])-1]
                                    num+=1
                                    last_time+=interval
                            total=rate
                            total_num=1


                #读端口
                lines = file_read.readlines()
                if(len(lines)>1):
                    last_time = 0
                    line = lines[1].split(',')
                    start= float(line[0])
                    total = 0
                    total_num = 0
                    num = 0
                    
                    if(port==0):
                        while(last_time+interval<start):
                            time2[i].append(last_time*Value)
                            read[i].append(0)
                            last_time+=interval
                            num+=1
                    else:
                        while(last_time+interval<start):
                            last_time+=interval
                            num+=1

                    for line in lines[1:]:
                        line = line.split(',')
                        if len(line) <5:
                            continue
                        time = float(line[0])
                        rate = float(line[4])/Gbps

                        if time <= last_time+interval:
                            total+=rate
                            total_num+=1
                        elif total_num ==0:
                            n = int((time - last_time)/interval)
                            if n>=4: #如果两个时间点之间间隔过长，则不再继承前面的值
                                for j in range(0,n):
                                    if(len(time2[i]))<=num:
                                        time2[i].append(last_time*Value)
                                        read[i].append(0)
                                    last_time+=interval
                                    num+=1
                            else:
                                for j in range(0,n):
                                    if(len(time2[i]))<=num:
                                        time2[i].append(last_time*Value)
                                        read[i].append(read[i][len(read[i])-1])
                                    else:
                                        read[i][num]+=read[i][len(read[i])-1]
                                    last_time+=interval
                                    num+=1
                            total+=rate
                            total_num+=1
                        else:
                            if(len(time2[i]))<=num:
                                time2[i].append(last_time*Value)
                                read[i].append(total/total_num)
                            else:
                                read[i][num]+=total/total_num
                            num+=1
                            total=0
                            total_num=0
                            last_time+=interval
                    if total!=0:
                        if len(time2[i]) <= num:
                            time2[i].append(last_time*Value)
                            read[i].append(total/total_num)
                        else:
                            read[i][num]+=total/total_num

        #丢包
        file_loss=[filename_loss,filename_loss_BMS]
        with open(file_loss[i]) as file_loss:
            lines = file_loss.readlines()
            if(len(lines)>1):
                last_time = 0
                line = lines[0].split(',')
                start= float(line[0])
                total = 0
                total_num = 0
                num=0
                flag = 0

                for line in lines:
                    line = line.split(',')
                    if len(line) <3:
                        continue
                    time = float(line[0])
                    loss1 = float(line[2])/1000
                    if(loss1==0 and flag==0): #直到第一次出现非零的loss才停止continue
                        continue
                    flag = 1

                    time3[i].append(time*Value)
                    if len(loss[i]) ==0:
                        loss[i].append(loss1)
                    else:
                        loss[i].append(loss[i][len(loss[i])-1]+loss1)

                interval0 =1e-6
                end_time =4
                while len(time3[i])>=1 and time3[i][-1]<end_time:
                    loss[i].append(loss[i][-1])
                    time3[i].append(time3[i][-1]+interval0*Value)

                    
    # plt.xlim(-0.1,1.2)
    plt.tick_params(axis='x',direction = 'in')
    plt.xticks(fontsize = xy_valaue*0.8)
    plt.xlabel('Time(ms)',fontsize=25)

    plt.ylim(-20,800)
    plt.tick_params(axis='y',direction = 'in')
    plt.yticks(fontsize = xy_valaue*0.8)
    plt.ylabel('DRAM Throughput(Gbps)',fontsize=25)

    ax=plt.gca()
    ax.xaxis.set_major_locator(MultipleLocator(0.4))
    ax.yaxis.set_major_locator(MultipleLocator(200))

    plt.plot(time1[1],write[1],label='DeepHir-Write Thr',linewidth=linewidth,color='r',marker='v',markersize=5)
    plt.plot(time2[1],read[1],label='DeepHir-Read Thr',linewidth=linewidth,color='r',linestyle='--',marker='^',markersize=5)
    plt.plot(time1[0],write[0],label='FBM-Write Thr',linewidth=linewidth,color='k',marker='.',markersize=5)
    plt.plot(time2[0],read[0],label='FBM-Read Thr',linewidth=linewidth,color='k',linestyle='--',marker='*',markersize=5)
    
    # ax2=ax.twinx()
    # #ax2.set_ylim(-0.5,8)
    # ax2.set_ylabel('Loss(MB)',fontsize=25)
    # ax2.tick_params(labelsize=25,direction='in')
    # #ax2.yaxis.set_major_locator(MultipleLocator(2))
    # ax2.plot(time3[0],loss[0],label='FBM-loss',linewidth=linewidth,color='k',linestyle='',marker='x',markersize=1)
    # ax2.plot(time3[1],loss[1],label='DeepHir-loss',linewidth=linewidth,color='r',linestyle='',marker='x',markersize=1)

    line1,label1=ax.get_legend_handles_labels()
    # line2,label2=ax2.get_legend_handles_labels()

    if len(label1) == 4:
        str0 = ['FBM-Write Thr','DeepHir-Write Thr','FBM-Read Thr','DeepHir-Read Thr']
        for i in range(0,4):
            j = label1.index(str0[i])
            label1[j],label1[i] = label1[i],label1[j]
            line1[j],line1[i] = line1[i],line1[j]

    plt.subplots_adjust(top=0.8,right=0.8)
    plt.legend(handles = [line1[0],line1[1],line1[2],line1[3]],labels = [label1[0],label1[1],label1[2],label1[3]],
            loc=(0, 1.05),ncol=2,fontsize='large',frameon=False)
    legend = plt.gca().get_legend()    
    legend.get_frame().set_linewidth(0)

    path = save_path + '/compare'+'/'+id+'/'+'dram-total-throughput--BMS-vs-pbs'+'.png'
    plt.savefig(path)
    plt.clf()


def total_read_throughput(id):
    interval = 1e-5
    time_end = 2e-3
    Time = [[],[]]
    Total = [[],[]]
    num=6
    if id=='tc2-06':#tc2-06下只有四个read文件
        num=4
    for number in range(0,2):
        for port in range(0,num):
            filename_read = data_dir+id+'/'+'queue-hbm-read-throughput-test-'+id+'-p'+str(port)+'.csv'
            filename_read_BMS = data_dir_BMS+id+'/'+'queue-hbm-read-throughput-test-'+id+'-p'+str(port)+'.csv'
            filename_read_sram = data_dir+id+'/'+'queue-sram-read-throughput-test-'+id+'-p'+str(port)+'.csv'
            filename_read_sram_BMS = data_dir_BMS+id+'/'+'queue-sram-read-throughput-test-'+id+'-p'+str(port)+'.csv'
            read_dram = [filename_read,filename_read_BMS]
            read_sram = [filename_read_sram,filename_read_sram_BMS]
            with open(read_dram[number]) as read_dram,open(read_sram[number]) as read_sram:
                #sram
                sram = []
                lines=read_sram.readlines()
                start=0
                if len(lines) > 1:
                    line =lines[1].split(',')
                    start= float(line[0])
                num_sram = []
                total=0
                total_num=0

                last_time = 0
                while(last_time+interval<start):
                    num_sram.append(0)
                    sram.append(0)
                    last_time+=interval
                
                for line in lines[1:]:
                    line =line.split(',')
                    if len(line)<5:
                        continue
                    time0=float(line[0])
                    sram0=float(line[4])/Gbps
                    if time0 <= last_time+interval:
                        total+=sram0
                        total_num+=1
                    elif total_num ==0:
                        n = int((time0 - last_time)/interval)
                        for i in range(0,n):
                            num_sram.append(num_sram[len(num_sram)-1])
                            sram.append(sram[len(sram)-1])
                            last_time+=interval
                        total+=sram0
                        total_num+=1
                    else:
                        num_sram.append(total_num)
                        sram.append(total)
                        total=0
                        total_num=0
                        last_time+=interval

                while(last_time+interval<=time_end):
                    num_sram.append(0)
                    sram.append(0)
                    last_time+=interval

                #dram
                lines=read_dram.readlines()
                start=0
                if len(lines) > 1:
                    line =lines[1].split(',')
                    start= float(line[0])
                dram=[]
                num_dram = []
                total=0
                total_num=0

                last_time = 0
                while(last_time+interval<start):
                    num_dram.append(0)
                    dram.append(0)
                    last_time+=interval
                
                for line in lines[1:]:
                    line =line.split(',')
                    if len(line)<5:
                        continue
                    time0=float(line[0])
                    dram0=float(line[4])/Gbps
                    if time0 <= last_time+interval:
                        total+=dram0
                        total_num+=1
                    elif total_num ==0:
                        n = int((time0 - last_time)/interval)
                        for i in range(0,n):
                            num_dram.append(num_dram[len(num_dram)-1])
                            dram.append(dram[len(dram)-1])
                            last_time+=interval
                        total+=dram0
                        total_num+=1
                    else:
                        num_dram.append(total_num)
                        dram.append(total)
                        total=0
                        total_num=0
                        last_time+=interval
                
                while(last_time+interval<=time_end):
                    num_dram.append(0)
                    dram.append(0)
                    last_time+=interval
                
                #total
                last_time = 0
                i = 0
                if port==0:
                    while(last_time+interval<=time_end):
                        Time[number].append(last_time*Value)
                        total0 =sram[i]+dram[i]
                        num0=num_sram[i]+num_dram[i]
                        if(num0==0):
                            Total[number].append(0)
                        else:
                            Total[number].append(total0/num0)
                        i+=1
                        last_time+=interval
                else:
                    while(last_time+interval<=time_end):
                        total0 =sram[i]+dram[i]
                        num0=num_sram[i]+num_dram[i]
                        if(num0!=0):
                            Total[number][i]+=(total0/num0)
                        i+=1
                        last_time+=interval
    
    plt.xlim(-0.1,2)
    plt.tick_params(axis='x',direction = 'in')
    plt.xticks(fontsize = xy_valaue*0.8)
    plt.xlabel('Time(ms)',fontsize=25)

    plt.ylim(-15,600)
    plt.tick_params(axis='y',direction = 'in')
    plt.yticks(fontsize = xy_valaue*0.8)
    plt.ylabel('Total Read Throughput(Gbps)',fontsize=25)

    ax=plt.gca()
    ax.xaxis.set_major_locator(MultipleLocator(0.4))
    ax.yaxis.set_major_locator(MultipleLocator(100))
    plt.plot(Time[0],Total[0],label='FBM',linewidth=linewidth,color='k')
    plt.plot(Time[1],Total[1],label='DeepHir',linewidth=linewidth,color='r')

    plt.legend(frameon=False)
    legend = ax.get_legend()
    legend.get_frame().set_linewidth(0)
    path = save_path + '/compare'+'/'+id+'/'+'total-read-throughput--BMS-vs-pbs'+'.png'
    plt.savefig(path)
    plt.clf()
            


### main part.
if not os.path.exists(save_path):
    os.makedirs(save_path)

if len(sys.argv) == 1:
    print("请传入测试用例编号，example:tc2-04")
else:
    testcase_number = sys.argv[1] #测试用例编号
    
    buffer_loss_compare(testcase_number)
    buffer_usage_plot(testcase_number)
    hbm_throughput_plot(testcase_number)
    sram_throughput_plot(testcase_number)
    get_loss_delay(testcase_number)
    
    total_sram_usage(testcase_number)
    Dram_total_throuphput(testcase_number)
    total_read_throughput(testcase_number)




