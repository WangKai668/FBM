import matplotlib.pyplot as plt
import numpy as np
from matplotlib.pyplot import MultipleLocator 
import os
import math
import csv
import re
import sys
import random
import bisect
from mpl_toolkits.axes_grid1 import host_subplot
from mpl_toolkits import axisartist
font_size =22
linewidth=1

plt.rc('font',family='Times New Roman')
# 当前脚本所在目录：tests/analysis
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# tests目录
TESTS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))

# 准备数据
x = [0, 0, 0.21, 0.21, 0.23, 0.23, 0.5, 0.5, 1, 1, 1.4, 1.4, 1.43, 1.43, 1.5, 1.5, 2.0]
y = [0, 400, 400, 1300, 1300, 400, 400, 0, 0, 200, 200, 1100, 1100, 200, 200, 0, 0]
x1 = [0, 0, 0.5, 0.5, 1, 1, 1.5, 1.5, 2]
y1 = [0, 200, 200, 0, 0, 200, 200, 0, 0]
x2 = [0, 0.21, 0.21, 0.23, 0.23, 1.4, 1.4, 1.43, 1.43, 2.0]
y2 = [0, 0, 1000, 1000, 0, 0, 900, 900, 0, 0]
x3 = [0, 0, 0.5, 0.5, 2.0]
y3 = [0, 200, 200, 0, 0]


# 片外绘制丢包率
Value = 1000
hw_loss_packet = os.path.join(
    TESTS_DIR,
    "data",
    "BMS",
    "tc2-05",
    "loss_packet.csv"
)
with open(hw_loss_packet) as file:
    lines = file.readlines()

    time1 = []
    loss = []
    flag = 1

    for line in lines:
        if len(line) < 5:
            continue
        line = line.split(',')
        try:
            cur = float(line[1])
        except ValueError:
            continue
        if flag == 1 and float(line[2]) == 0:
            loss.append(float(line[5]))
            time1.append(cur * Value)
            continue
        flag = 0
        if len(line) == 6:
            if len(loss) > 1:
                loss.append(float(line[5]))
                time1.append(cur * Value)
            else:
                loss.append(float(line[5]))
                time1.append(cur * Value)



#cyx的

plt.xlabel('Time (ms)',fontsize=font_size)
plt.ylabel('Rate (Gbps)',fontsize=font_size)
plt.xticks(fontsize=font_size) 
plt.yticks(fontsize=font_size)  
plt.tick_params(axis='x', direction='in')
plt.tick_params(axis='y', direction='in')
ax=plt.gca()
ax.xaxis.set_major_locator(MultipleLocator(0.4))

plt.plot(x1,y1,label='port1', color='red',marker='*',linewidth=3)
plt.plot(x2, y2, label='port2', linestyle='--', color='green',marker='d',linewidth=3)
plt.plot(x3, y3, label='port3', linestyle='-.', color='orange',marker='^',linewidth=3)

ax2=ax.twinx()
ax2.set_ylim(-25, 175)
ax2.set_ylabel('# of Packet Loss', fontsize=font_size)
ax2.tick_params(axis='y', direction='in', labelsize=font_size)

ax2.plot(time1, loss, label='Loss', linewidth=1, color='blue',marker='.')

handles1,labels1=ax.get_legend_handles_labels()
handles2,labels2=ax2.get_legend_handles_labels()

plt.subplots_adjust(top=0.85)
plt.legend(handles1+handles2,labels1+labels2,loc=(0.08,1),ncol=2,fontsize=font_size)
legend =plt.gca().get_legend()#获取图表中的图例对象
legend.get_frame().set_linewidth(0)#使用legend.get_frame()方法来获取图例框对象，并通过set_linewidth(0)方法将图例框的边框宽度设置为0，即消除了图例框的边框线。

plt.tight_layout()

path = os.path.join(
    TESTS_DIR,
    "data1.pdf"
)
plt.savefig(path)
plt.clf()
