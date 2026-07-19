import sys
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

	# -c FbHdp_distribution.txt -n 30 -l 0.8 -t 0.02 -b 100G
	# websearched -n 64 -t 0.02 -l 0.9 -b 100G  

	myArgs=input("请输入参数：")
	myArgs=myArgs.split(" ")
	options,args = parser.parse_args(myArgs)

	base_t = 2000000000

	if not options.nhost:
		print( "please use -n to enter number of hosts")
		sys.exit(0)

	nhost = int(options.nhost)
	receiver = 12  # 手动指定接收端数目
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

	ofile = open(output, "w")

	# generate flows
	avg = customRand.getAvg()
	avg_inter_arrival = 1/(bandwidth*load/8./avg)*1000000000 * ((nhost - receiver) / receiver) # bandwidth: bps;    avg: B;   avg_inter_arrival: ns
	print(bandwidth, avg)
	n_flow_estimate = int(time / avg_inter_arrival * (nhost - receiver)) # 预估总流数量 = (总时间/平均间隔时间) * 发送端数目
	n_flow = 0
	print(n_flow_estimate, nhost)
	print(f"n_flow_estimate={n_flow_estimate}")

	host_list = [(base_t + int(poisson(avg_inter_arrival)), i) for i in range(receiver,nhost)] # 每个主机给一个发送时间
	heapq.heapify(host_list)

	dataTime = 0

	i = 1
	while i <= n_flow_estimate:
		i+=1
		t,src = host_list[0] # 获取一个主机及其发送时间
		inter_t = int(poisson(avg_inter_arrival)) # 发送间隔
		new_tuple = (src, t + inter_t)
		dst = random.randint(0, receiver-1) # 随机指定一个接收端
		#while (dst == src):
		#	dst = random.randint(0, nhost-1)
		if (t + inter_t > time + base_t):
			heapq.heappop(host_list) # 该发送端发送完毕，从host_list剔除
		else:
			size = int(customRand.rand())
			if size <= 0:
				size = 1
			n_flow += 1
			# ofile.write("%d %d 3 100 %d %.9f\n"%(src, dst, size, t * 1e-9))
			# 原tcp输出是（来源端口，目的端口，包大小，时间长度） udp需要改成（来源端口，目的端口，开始时间，结束时间，传送速率）
			dataTime = t * 1e-9 # 更新时间
			#while (dst == 0):
			#	dst = random.randint(1, nhost-1)
			ofile.write("%d %d %.9f %.9f %s\n"%(src, dst, dataTime , dataTime + size*8/bandwidth , str(int(bandwidth/1000000000))+'Gbps' ))
			heapq.heapreplace(host_list, (t + inter_t, src)) # 更新host_list，按发送时间排序

	
	print(f"流量文件生成完毕，位于{output}")

	ofile.seek(0)
	ofile.write("%d"%n_flow)
	ofile.close()

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
