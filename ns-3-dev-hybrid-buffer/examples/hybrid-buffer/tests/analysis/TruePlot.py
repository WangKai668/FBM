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

data_dir =f'/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/'
save_path = f'/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data-fig/'

def get_last_field(file_name):
    with open(file_name, 'rb') as f:
        f.seek(-2, 2)  # 移动到倒数第二个字节
        while f.read(1) != b'\n':  # 读到换行符为止
            f.seek(-2, 1)  # 继续向前移动
        last_line = f.readline().decode()  # 读取最后一行
        # print(f'{file_name}\n{last_line}')
    return last_line.strip().split(',')[-1]  # 获取最后三项

def get_mean(file_name):
    with open(file_name, 'r') as file: 
        reader = csv.reader(file)
        next(reader)
        throughput = [int(row[-1]) for row in reader]
        mean = sum(throughput) / len(throughput)
        return mean

def test9_plot(testcase='tc2-04'):
    print("test9_plot被调用，绘制所有需要的图【包含BMS，pbs】")
    print(f'testcase={testcase}')
    ax_data = [] #丢包loss

    # Deephir
    thresholds = [0.2,0.5,1.0,2.0,4.0]
    file_path_pre = data_dir + 'BMS/'+testcase+'/'

    for threshold in thresholds:
        file_path = file_path_pre + str(threshold) +'M/loss_packet.csv'
        
        loss_num = float(get_last_field(file_path)) # 打开文件并读取最后一行的最后一项
        ax_data.append(loss_num)
        #file_path = file_path_pre + str(threshold) +''
    
    #FBM
    file_path_pre = data_dir + 'pbs/'+testcase+'/'
    # for flow_rate in flow_rates:
    file_path = file_path_pre  +'loss_packet.csv'
    
    loss_num = float(get_last_field(file_path)) # 打开文件并读取最后一行的最后一项
    ax_data.append(loss_num)

    #打印Deephir和FBM的结果，复制到realPlot中画图。
    print(ax_data,1)

if __name__ == "__main__":
    import sys
    testcase = sys.argv[1] if len(sys.argv) > 1 else 'tc2-04'
    test9_plot(testcase)