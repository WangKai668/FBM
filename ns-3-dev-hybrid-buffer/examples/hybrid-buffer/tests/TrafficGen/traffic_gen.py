import sys
import os
import random
import math
import heapq
from optparse import OptionParser
from custom_rand import CustomRand

class Flow:
	def __init__(self, src, dst, size, t):
		self.src, self.dst, self.size, self.t = src, dst, size, t
	def __str__(self):
		return "%d %d 3 100 %d %.9f"%(self.src, self.dst, self.size, self.t)


def randNorm(start,end,avg,sig):
	while True :
		x=random.gauss(avg,sig)
		if start <= x <= end :
			if x >= 600 :
				x += 200
			elif x <= 300:
				x += 400
			return x


def translate_bandwidth(b):
	if b == None:
		return None
	if type(b)!=str:
		return None
	if b[-1] == 'G':
		return float(b[:-1])*1e9
	if b[-1] == 'M':
		return float(b[:-1])*1e6
	if b[-1] == 'K':
		return float(b[:-1])*1e3
	return float(b)

def poisson(lam):
	return -math.log(1-random.random())*lam

if __name__ == "__main__":
	port = 80
	parser = OptionParser()
	parser.add_option("-c", "--cdf", dest = "cdf_file", help = "the file of the traffic size cdf", 
				   default = 
				   #"uniform_distribution.txt"
				#    "cdfW1.txt"
				"WebSearch_distribution.txt"
				   )
	parser.add_option("-n", "--nhost", dest = "nhost", help = "number of hosts")
	parser.add_option("-l", "--load", dest = "load", help = "the percentage of the traffic load to the network capacity, by default 0.3", default = "0.3")
	parser.add_option("-b", "--bandwidth", dest = "bandwidth", help = "the bandwidth of host link (G/M/K), by default 10G", default = "10G")
	parser.add_option("-t", "--time", dest = "time", help = "the total run time (s), by default 10", default = "10")
	parser.add_option("-o", "--output", dest = "output", help = "the output file", default = "Generated/traffic.txt")

	# fakeArgs=['-c','cdfW1.txt','-n',320,'-l',0.3,'-b','100G','-t',0.1]
	# -c FbHdp_distribution.txt -n 30 -l 0.9 -t 0.004 -b 100G
	# websearched -n 64 -t 0.030 -l 0.8 -b 100G

	myArgs=input("请输入参数：")
	myArgs=myArgs.split(" ")

	options,args = parser.parse_args(myArgs)

	base_t = 2000000000
	receiver = 12

	if not options.nhost:
		print( "please use -n to enter number of hosts")
		sys.exit(0)
	nhost = int(options.nhost)
	load = float(options.load)
	bandwidth = translate_bandwidth(options.bandwidth)
	time = float(options.time)*1e9 # translates to ns
	output = options.output
	if bandwidth == None:
		print( "bandwidth format incorrect")
		sys.exit(0)

	fileName = options.cdf_file
	file = open(fileName,"r")
	lines = file.readlines()
	# read the cdf, save in cdf as [[x_i, cdf_i] ...]
	cdf = []
	for line in lines:
		x,y = map(float, line.strip().split(' '))
		cdf.append([x,y])
		#print x,y

	# create a custom random generator, which takes a cdf, and generate number according to the cdf
	customRand = CustomRand()
	if not customRand.setCdf(cdf):
		print("Error: Not valid cdf")
		#sys.exit(0)

	# generate flows
	# 获取流大小平均值，单位 Byte
	avg = customRand.getAvg()

	# 发送端数量：
	# 0～5 是接收端，6～29 是发送端
	num_senders = nhost - receiver

	if num_senders <= 0:
		print("错误：发送端数量必须大于0")
		sys.exit(1)
	# 平均每个接收端对应多少个发送端
	# nhost=30、receiver=6 时，fan_in=24/6=4
	fan_in = num_senders / receiver
	# 目标 load 表示接收端出口链路负载。
	# 平均4个发送端打向1个接收端，因此单发送端的到达间隔放大4倍。
	avg_inter_arrival = (1 / (bandwidth * load / 8.0 / avg)* 1e9* fan_in)

	n_flow_estimate = int(time / avg_inter_arrival * num_senders)

	print(
		"bandwidth:", bandwidth,
		"avg_size:", avg,
		"num_senders:", num_senders,
		"receivers:", receiver,
		"fan_in:", fan_in,
		"avg_itv:", avg_inter_arrival
	)

	print("n_flow_estimate =", n_flow_estimate)

	# 每个发送端的第一次流到达时间
	host_list = [(base_t + max(1, int(poisson(avg_inter_arrival))), src) for src in range(receiver, nhost)]

	heapq.heapify(host_list)

	# 流量结束时间
	end_t = base_t + time

	# 保存所有生成的流
	flows = []

	while host_list:
		# 取下一条最早到达的流
		t, src = heapq.heappop(host_list)

		# 所有剩余到达时间都已超过结束时间
		if t > end_t:
			break

		# 随机选择0～5号接收端
		dst = random.randint(0, receiver - 1)

		# 按CDF随机生成流大小
		size = int(customRand.rand())
		if size <= 0:
			size = 1

		# ns转为秒
		start_time = t * 1e-9

		flows.append((src, dst, start_time, size))

		# 为该发送端生成下一条流的到达时间
		inter_t = max(1, int(poisson(avg_inter_arrival)))
		next_t = t + inter_t

		if next_t <= end_t:
			heapq.heappush(host_list, (next_t, src))

	# 确保输出目录存在
	import os

	output_dir = os.path.dirname(output)
	if output_dir:
		os.makedirs(output_dir, exist_ok=True)

	# 一次性写入文件，避免覆盖第一条流
	with open(output, "w") as ofile:
		# 第一行：实际流数量
		ofile.write(f"{len(flows)}\n")

		# 后续每行：src dst start_time size
		for src, dst, start_time, size in flows:
			ofile.write(
				f"{src} {dst} {start_time:.9f} {size}\n"
			)

	print("实际生成流数量：", len(flows))
	print(f"流量文件生成完毕，位于 {output}")

'''
	f_list = []
	avg = customRand.getAvg()
	avg_inter_arrival = 1/(bandwidth*load/8./avg)*1000000000
	# print avg_inter_arrival
	for i in range(nhost):
		t = base_t
		while True:
			inter_t = int(poisson(avg_inter_arrival))
			t += inter_t
			dst = random.randint(0, nhost-1)
			while (dst == i):
				dst = random.randint(0, nhost-1)
			if (t > time + base_t):
				break
			size = int(customRand.rand())
			if size <= 0:
				size = 1
			f_list.append(Flow(i, dst, size, t * 1e-9))

	f_list.sort(key = lambda x: x.t)

	print len(f_list)
	for f in f_list:
		print f
'''
