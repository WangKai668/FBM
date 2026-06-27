import sys
sys.path.append("../../../../utils/plot")

import os
import plot as myplot
myplot.plt.rcParams.update({'figure.max_open_warning': 0})

data_dir =f'/home/dell6/yrf/pba/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data'
bf_file_pre = f'{data_dir}/buffer-usage-'
hbmth_file_pre = f'{data_dir}/hbm-throughput-'
pth_file_pre = f'{data_dir}/port-throughput-'
wcacheth_file_pre = f'{data_dir}/wcache-throughput-'
sramth_file_pre = f'{data_dir}/sram-throughput-'
loss_packet = f'{data_dir}/loss_packet.csv'
cost_etc_test_port0 = f'{data_dir}/cost_etc_test_port0.csv'
cost_etc_test_port1 = f'{data_dir}/cost_etc_test_port1.csv'

qth_file_pre = f'{data_dir}/queue-throughput-'
qwcache_usage_file_pre = f'{data_dir}/queue-wcache-usage-'
qwcache_read_throughput_file_pre = f'{data_dir}/queue-wcache-read-throughput-'
qwcache_write_throughput_file_pre = f'{data_dir}/queue-wcache-write-throughput-'
qsram_usage_file_pre = f'{data_dir}/queue-sram-usage-'
qsram_read_throughput_file_pre = f'{data_dir}/queue-sram-read-throughput-'
qsram_write_throughput_file_pre = f'{data_dir}/queue-sram-write-throughput-'
qhbm_usage_file_pre = f'{data_dir}/queue-hbm-usage-'
qhbm_read_throughput_file_pre = f'{data_dir}/queue-hbm-read-throughput-'
qhbm_write_throughput_file_pre = f'{data_dir}/queue-hbm-write-throughput-'


savepath = f'/home/dell6/yrf/pba/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data-fig' #savepath = f'/home/slyang/tmp-log/final_fig'

nPort = 64
nPrio = 2
nQueue = 5

interval = 100
KB = 1024
MB = KB*1024
GB = MB*1024
Gbps = 1000*1000*1000
ns = 1000*1000*1000

figstar=0.1
figend=0.1005

### Buffer Usage Part.
def buffer_usage_plot(filename, id):
    # Buffer usage (SRAM/Wcache/DRAM)
    with open(filename) as file:
        next(file)

        time = []
        sram = []
        wcache = []
        dram = []
        lineNum = 0
        for line in file:
            lineNum += 1
            if lineNum % interval == 0 or lineNum // interval < 2:
                line = line.split(',')
                if len(line) < 4:
                    continue
                time.append(float(line[0]))
                sram.append(float(line[1])/MB)
                wcache.append(float(line[2])/KB)
                dram.append(float(line[3][:-1])/MB)    # remove \n at the end of line

        lp = myplot.LineDashPlot()
        myplot.plt.xlim(figstar,figend)
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Buffer Usage(MB)')
        lp.plot(time, sram, label='SRAM')
        png = 'buffer-usage-' + id + '-SRAM' + '.png'
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()

        lp = myplot.LineDashPlot()
        #myplot.plt.xlim(figstar,figend)
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Buffer Usage(KB)')
        lp.plot(time, wcache, label='Wcache')
        png = 'buffer-usage-' + id + '-Wcache' + '.png'
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()

        lp = myplot.LineDashPlot()
        myplot.plt.xlim(figstar,figend)
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Buffer Usage(MB)')
        lp.plot(time, dram, label='DRAM')
        png = 'buffer-usage-' + id + '-DRAM' + '.png'
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()

def queue_wcache_usage_plot(file_pre, id):
    # Port/Queue throughput
    wcache_usage = {}
    filename = file_pre + ".csv"
    if not os.path.isfile(filename):
        return

    with open(filename) as file:
        lines = file.readlines()
        if len(lines) <= 1:
            return

        lineNum = 0
        inter = interval
        queueNum = 0

        for line in lines[1:]:
            lineNum += 1
            if lineNum % inter == 0 or lineNum // interval < 2:
                line = line.split(',')
                if len(line) < 5:
                    continue
                time0 = (float(line[0]))
                wcache0 = (float(line[4])/KB)
                port = line[1]
                prio = line[2]
                queue = line[3]

                if (port, prio, queue) not in wcache_usage.keys():
                    time = []
                    wcache = []
                    wcache_usage[(port, prio, queue)] = (time, wcache)
                    queueNum += 1
                    inter = round(interval / queueNum)
                else:
                    wcache_usage[(port, prio, queue)][0].append(time0)
                    wcache_usage[(port, prio, queue)][1].append(wcache0)

    if len(wcache_usage) == 0:
        return
    for key in wcache_usage.keys():
        lp = myplot.LineDashPlot()
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('WCache_Usage(KB)')
        port = key[0]
        prio = key[1]
        queue = key[2]
        label = 'P' + str(port) + '-Pr' + str(prio) + '-Q' + str(queue)

        lp.plot(wcache_usage[key][0], wcache_usage[key][1], label=label)

        png = 'queue-wcache-usage-' + id + label +  '.png'
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()

def queue_sram_usage_plot(file_pre, id):
    # Port/Queue throughput
    sram_usage = {}
    filename = file_pre + ".csv"
    if not os.path.isfile(filename):
        return

    with open(filename) as file:
        lines = file.readlines()
        if len(lines) <= 1:
            return

        lineNum = 0
        inter = interval
        queueNum = 0

        for line in lines[1:]:
            lineNum += 1
            if lineNum % inter == 0 or lineNum // interval < 2:
                if len(line) < 5:
                    continue
                line = line.split(',')
                time0 = (float(line[0]))
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
        lp = myplot.LineDashPlot()
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Sram_Usage(MB)')
        port = key[0]
        prio = key[1]
        queue = key[2]
        label = 'P' + str(port) + '-Pr' + str(prio) + '-Q' + str(queue)
        lp.plot(sram_usage[key][0], sram_usage[key][1], label=label)

        png = 'queue-sram-usage-' + id + label + '.png'
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()

def queue_hbm_usage_plot(file_pre, id):
    # Port/Queue throughput
    hbm_usage = {}
    filename = file_pre + ".csv"
    if not os.path.isfile(filename):
        return

    with open(filename) as file:
        lines = file.readlines()
        if len(lines) <= 1:
            return

        lineNum = 0
        inter = interval
        queueNum = 0

        for line in lines[1:]:
            lineNum += 1
            if lineNum % inter == 0 or lineNum // interval < 2:
                line = line.split(',')
                if len(line) < 5:
                    continue
                time0 = (float(line[0]))
                hbm0 = (float(line[4])/GB)
                port = line[1]
                prio = line[2]
                queue = line[3]

                if (port, prio, queue) not in hbm_usage.keys():
                    time = []
                    hbm = []
                    hbm_usage[(port, prio, queue)] = (time, hbm)
                    queueNum += 1
                    inter = round(interval / queueNum)
                else:
                    hbm_usage[(port, prio, queue)][0].append(time0)
                    hbm_usage[(port, prio, queue)][1].append(hbm0)


    if len(hbm_usage) == 0:
        return

    for key in hbm_usage.keys():
        lp = myplot.LineDashPlot()
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Hbm_Usage(GB)')
        port = key[0]
        prio = key[1]
        queue = key[2]
        label = 'P' + str(port) + '-Pr' + str(prio) + '-Q' + str(queue)
        lp.plot(hbm_usage[key][0], hbm_usage[key][1], label=label)

        png = 'queue-hbm-usage-' + id + label + '.png'
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()


### Throughput Part
def hbm_throughput_plot(filename, id):
    # HBM throughput
    with open(filename) as file:
        lines = file.readlines()
        if len(lines) <= 1:
            return

        time = []
        read = []
        write = []
        line1 = lines[1].split(',')
        start = float(line1[0])
        inter = float(line1[1]) - start
        time.append(start)
        read.append(0)
        write.append(0)
        for line in lines[1:]:
            if len(line) < 4:
                continue
            line = line.split(',')
            cur = float(line[1])
            time.append(cur)
            read.append(float(line[2])/Gbps)
            write.append(float(line[3][:-1])/Gbps)

    with open(loss_packet) as file:
        lines = file.readlines()
        if len(lines) <= 1:
            return

        time1 = []
        loss = []
        line1 = lines[1].split(',')
        start = float(line1[0])
        inter = float(line1[1]) - start
        time1.append(start)
        loss.append(0)
        for line in lines[1:]:
            if len(line) < 4:
                continue
            line = line.split(',')
            try:
                cur = float(line[1])
            except ValueError:
                continue
            time1.append(cur)
            loss.append(float(line[2]))

        lp = myplot.LineDashPlot()
        myplot.plt.xlim(figstar,figend)
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Throughput(Gbps)')
        lp.plot(time, read, label='Read')
        lp.plot(time, write, label='Write')
        lp.plot(time1, loss, label='Loss')
        png = 'hbm-throughput-' + id + '.png'
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()

def wcache_throughput_plot(filename, id):

    with open(cost_etc_test_port0) as file:
        lines = file.readlines()
        if len(lines) <= 1:
            return

        port0_time2 = []
        port0_cost_min_s = []
        port0_cost_min_d = []
        port0_etc=[]
        # 
        # etc_d=[]

        line1 = lines[1].split(',')
        start = float(line1[0])
        inter = float(line1[1]) - start
        port0_time2.append(start)
        port0_cost_min_s.append(0)
        port0_cost_min_d.append(0)
        port0_etc.append(0)
        # cost_min_d.append(0)
        # etc_d.append(0)

        for line in lines[1:]:
            if len(line) < 4:
                continue
            line = line.split(',')
            try:
                cur = float(line[1])
            except ValueError:
                continue
            port0_time2.append(cur)
            port0_cost_min_s.append(float(line[2]))
            port0_cost_min_d.append(float(line[3]))
            port0_etc.append(float(line[4]))
            # cost_min_d.append(float(line[3]))
            # etc_d.append(float(line[5]))
        
        lp = myplot.LineDashPlot()
        myplot.plt.xlim(0.1,0.1005)
        myplot.plt.ylim(0,0.2)
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Value')
        lp.plot(port0_time2, port0_cost_min_s, label='port0_cost_min_s')
        lp.plot(port0_time2, port0_cost_min_d, label='port0_cost_min_d')
        lp.plot(port0_time2, port0_etc, label='port0_etc')
        # lp.plot(time2, etc_s, label='etc_d')
        png = 'Port0_Cost_etc-' + id + '.png'
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()



    with open(cost_etc_test_port1) as file:
        lines = file.readlines()
        if len(lines) <= 1:
            return

        port1_time2 = []
        port1_cost_min_s = []
        port1_cost_min_d = []
        port1_etc=[]
        # 
        # etc_d=[]

        line1 = lines[1].split(',')
        start = float(line1[0])
        inter = float(line1[1]) - start
        port1_time2.append(start)
        port1_cost_min_s.append(0)
        port1_cost_min_d.append(0)
        port1_etc.append(0)
        # cost_min_d.append(0)
        # etc_d.append(0)

        for line in lines[1:]:
            if len(line) < 4:
                continue
            line = line.split(',')
            try:
                cur = float(line[1])
            except ValueError:
                continue
            port1_time2.append(cur)
            port1_cost_min_s.append(float(line[2]))
            port1_cost_min_d.append(float(line[3]))
            port1_etc.append(float(line[4]))
            # cost_min_d.append(float(line[3]))
            # etc_d.append(float(line[5]))
        




        lp = myplot.LineDashPlot()
        myplot.plt.xlim(0.100,0.1005)
        myplot.plt.ylim(0,0.2)
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Value')
        lp.plot(port0_time2, port0_cost_min_s, label='port0_cost_min_s')
        lp.plot(port0_time2, port0_cost_min_d, label='port0_cost_min_d')
        lp.plot(port0_time2, port0_etc, label='port0_etc')

        lp.plot(port1_time2, port1_cost_min_s, label='port1_cost_min_s')
        lp.plot(port1_time2, port1_cost_min_d, label='port1_cost_min_d')
        lp.plot(port1_time2, port1_etc, label='port1_etc')
        # lp.plot(time2, etc_s, label='etc_d')
        png = 'Port1_Cost_etc-' + id + '.png'
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()
        

def sram_throughput_plot(filename, id):
    # HBM throughput
    with open(filename) as file:
        lines = file.readlines()
        
        if len(lines) <= 1:
            return

        time = []
        read = []
        write = []
        line1 = lines[1].split(',')
        start = float(line1[0])
        inter = float(line1[1]) - start
        time.append(start)
        read.append(0)
        write.append(0)
        for line in lines[1:]:
            if len(line) < 4:
                continue
            line = line.split(',')
            cur = float(line[1])
            time.append(cur)
            read.append(float(line[2])/Gbps)
            write.append(float(line[3][:-1])/Gbps)

        lp = myplot.LineDashPlot()
        myplot.plt.xlim(figstar,figend)
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Throughput(Gbps)')
        lp.plot(time, read, label='Read')
        lp.plot(time, write, label='Write')
        png = 'sram-throughput-' + id + '.png'
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf() 
        
def queue_throughput_plot(file_pre, id):
    # Port/Queue throughput
    thput = {}
    for port in range(nPort):
        for prio in range(nPrio):
            for queue in range(nQueue):
                filename = file_pre + "-p" + str(port) + "-pri" + str(prio) + "-q" + str(queue) + ".csv"
                if not os.path.isfile(filename):
                    continue

                with open(filename) as file:
                    lines = file.readlines()
                    if len(lines) <= 1:
                        continue

                    line1 = lines[1].split(',')
                    line_1 = lines[-1].split(',')
                    start0 = round(float(line1[0])*ns)
                    end0 = round(float(line_1[1])*ns)
                    inter = round(float(line1[1])*ns - start0)
                    time = [t/ns for t in range(start0, end0+inter, inter)]

                    for line in lines[1:]:
                        line = line.split(',')
                        if len(line) < 3:
                            continue
                        start = float(line[0])*ns
                        end = float(line[1])*ns
                        rate = float(line[2][:-1])/Gbps
                        if (port, prio, queue) not in thput.keys():
                            time_t = []
                            thput_t = [0]*len(time)
                            thput[(port, prio, queue)] = (time, thput_t)
                        else:
                            thput[(port, prio, queue)][1][round((int(end)-start0)/inter)] = rate
    
    if len(thput) == 0:
        return
    for key in thput.keys():
        lp = myplot.LineDashPlot()
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Throughput(Gbps)')
        port = key[0]
        prio = key[1]
        queue = key[2]
        label = 'P' + str(port) + '-Pr' + str(prio) + '-Q' + str(queue)
        lp.plot(thput[key][0], thput[key][1], label=label)

        png = 'queue-throughput-' + id + label + '.png'
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()

def queue_read_throughput_plot(file_pre, id):
    # Port/Queue throughput
    read_thput = {}
    for port in range(nPort):
        filename = file_pre + "-p" + str(port) + ".csv"
        if not os.path.isfile(filename):
            continue

        with open(filename) as file:
            lines = file.readlines()
            if len(lines) <= 1:
                continue

            line1 = lines[1].split(',')
            line_1 = lines[-1].split(',')
            start0 = round(float(line1[0])*ns)
            end0 = round(float(line_1[1])*ns)
            inter = round(float(line1[1])*ns - start0)
            time = [t/ns for t in range(start0, end0+inter, inter)]

            for line in lines[1:]:
                line = line.split(',')
                if len(line) < 5:
                    continue
                start = float(line[0])*ns
                end = float(line[1])*ns
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

    for key in read_thput.keys():
        lp = myplot.LineDashPlot()
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Throughput(Gbps)')
        port = key[0]
        prio = key[1]
        queue = key[2]
        label = 'P' + str(port) + '-Pr' + str(prio) + '-Q' + str(queue)
        lp.plot(read_thput[key][0], read_thput[key][1], label=label)

        png = file_pre + label + '.png'
        png = png.removeprefix(data_dir + '/')
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()



def queue_write_throughput_plot(file_pre, id):
    # Port/Queue throughput
    write_thput = {}
    for port in range(nPort):
        filename = file_pre + "-p"+ str(port) + ".csv"
        if not os.path.isfile(filename):
            continue

        with open(filename) as file:
            lines = file.readlines()
            if len(lines) <= 10:
                continue
            line1 = lines[1].split(',')
            line_1 = lines[-1].split(',')
            start0 = round(float(line1[0])*ns)
            end0 = round(float(line_1[1])*ns)
            inter = round(float(line1[1])*ns - start0)
            time = [t/ns for t in range(start0, end0+inter, inter)]

            for line in lines[1:]:
                line = line.split(',')
                if len(line) < 5:
                    continue
                start = float(line[0])*ns
                end = float(line[1])*ns
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

    for key in write_thput.keys():
        lp = myplot.LineDashPlot()
        myplot.plt.xlabel('Time(s)')
        myplot.plt.ylabel('Throughput(Gbps)')
        port = key[0]
        prio = key[1]
        queue = key[2]
        label = 'P' + str(port) + '-Pr' + str(prio) + '-Q' + str(queue)

        lp.plot(write_thput[key][0], write_thput[key][1], label=label)

        png = file_pre + label + '.png'
        png = png.removeprefix(data_dir + '/')
        myplot.plt.savefig(os.path.join(savepath, png))
        myplot.plt.clf()


### main part.
if not os.path.exists(savepath):
    os.makedirs(savepath)

tcscene = 6
tccases = 6
subtccs = 6
subsubtccs = 6 #100000000

for test in ['test-tc', 'test2-tc', 'test3-tc', ][0:1]:
    for i in range(1, tcscene + 1):
        for j in range(1, tccases + 1):
            id = test + str(i) + "-" + str(j).zfill(2)
            bf_filename = bf_file_pre + id + ".csv"
            if os.path.isfile(bf_filename):
                buffer_usage_plot(bf_filename, id)
                
            hbm_filename = hbmth_file_pre + id + ".csv"
            if os.path.isfile(hbm_filename):
                hbm_throughput_plot(hbm_filename, id) 
    
            wcache_filename = wcacheth_file_pre + id + ".csv"
            if os.path.isfile(wcache_filename):
                wcache_throughput_plot(wcache_filename, id)

            sram_filename = sramth_file_pre + id + ".csv"
            if os.path.isfile(sram_filename):
                sram_throughput_plot(sram_filename, id)

            # queue level wcache Usage Plot.
            q_file_pre = qth_file_pre + id
            queue_throughput_plot(q_file_pre, id)

            qwcache_usage_file_pre_t = qwcache_usage_file_pre + id
            queue_wcache_usage_plot(qwcache_usage_file_pre_t, id)

            qsram_usage_file_pre_t = qsram_usage_file_pre + id
            queue_sram_usage_plot(qsram_usage_file_pre_t, id)

            qhbm_usage_file_pre_t = qhbm_usage_file_pre + id
            queue_hbm_usage_plot(qhbm_usage_file_pre_t, id)

            qwcache_read_throughput_file_pre_t = qwcache_read_throughput_file_pre + id
            qwcache_write_throughput_file_pre_t = qwcache_write_throughput_file_pre + id
            queue_read_throughput_plot(qwcache_read_throughput_file_pre_t, id)
            queue_write_throughput_plot(qwcache_write_throughput_file_pre_t, id)

            qsram_read_throughput_file_pre_t = qsram_read_throughput_file_pre + id
            qsram_write_throughput_file_pre_t = qsram_write_throughput_file_pre + id
            queue_read_throughput_plot(qsram_read_throughput_file_pre_t, id)
            queue_write_throughput_plot(qsram_write_throughput_file_pre_t, id)

            qhbm_read_throughput_file_pre_t = qhbm_read_throughput_file_pre + id
            qhbm_write_throughput_file_pre_t = qhbm_write_throughput_file_pre + id
            queue_read_throughput_plot(qhbm_read_throughput_file_pre_t, id)
            queue_write_throughput_plot(qhbm_write_throughput_file_pre_t, id)
                
            for k in range(1, subtccs + 1):
                id = test + str(i) + "-" + str(j).zfill(2) + "-" + str(k).zfill(2)
                bf_filename = bf_file_pre + id + ".csv"
                if os.path.isfile(bf_filename):
                    buffer_usage_plot(bf_filename, id)
                    
                hbm_filename = hbmth_file_pre + id + ".csv"
                if os.path.isfile(hbm_filename):
                    hbm_throughput_plot(hbm_filename, id) 

                wcache_filename = wcacheth_file_pre + id + ".csv"
                if os.path.isfile(wcache_filename):
                    wcache_throughput_plot(wcache_filename, id)

                sram_filename = sramth_file_pre + id + ".csv"
                if os.path.isfile(sram_filename):
                    sram_throughput_plot(sram_filename, id)
                    
                q_file_pre = qth_file_pre + id
                queue_throughput_plot(q_file_pre, id)

                qwcache_usage_file_pre_t = qwcache_usage_file_pre + id
                queue_wcache_usage_plot(qwcache_usage_file_pre_t, id)

                qsram_usage_file_pre_t = qsram_usage_file_pre + id
                queue_sram_usage_plot(qsram_usage_file_pre_t, id)

                qhbm_usage_file_pre_t = qhbm_usage_file_pre + id
                queue_hbm_usage_plot(qhbm_usage_file_pre_t, id)

                qwcache_read_throughput_file_pre_t = qwcache_read_throughput_file_pre + id
                qwcache_write_throughput_file_pre_t = qwcache_write_throughput_file_pre + id
                queue_read_throughput_plot(qwcache_read_throughput_file_pre_t, id)
                queue_write_throughput_plot(qwcache_write_throughput_file_pre_t, id)

                qsram_read_throughput_file_pre_t = qsram_read_throughput_file_pre + id
                qsram_write_throughput_file_pre_t = qsram_write_throughput_file_pre + id
                queue_read_throughput_plot(qsram_read_throughput_file_pre_t, id)
                queue_write_throughput_plot(qsram_write_throughput_file_pre_t, id)

                qhbm_read_throughput_file_pre_t = qhbm_read_throughput_file_pre + id
                qhbm_write_throughput_file_pre_t = qhbm_write_throughput_file_pre + id
                queue_read_throughput_plot(qhbm_read_throughput_file_pre_t, id)
                queue_write_throughput_plot(qhbm_write_throughput_file_pre_t, id)
