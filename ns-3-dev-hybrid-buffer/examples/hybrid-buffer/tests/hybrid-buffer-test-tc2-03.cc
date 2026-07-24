/*
 * Copyright (c) 2022 Xi'an Jiaotong University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Yuqi Liu <liuyuqi526@126.com>
 */

/*
 * Network topology
 *
 *         n1            n3
 *            \        /
 * 1200Gbps,   \      /   100Gbps,
 * 1ms      .   \    /  . 1ms
 *          .     n0    .
 *          .   /    \  .
 *             /      \
 *            /        \
 *         n2            n4
 *
 * - all net devices are reorder-point-to-point net devices
 * - all links are point-to-point links with indicated one-way BW/delay
 * - DropTail queues with backpressure from NetDeviceQueueInterface
 * - Traffic: n1, n2 send 1-to-1 traffic to n3, n4. one is 1200Gbps congested
 *            flow to HP queue, another starts 1200Gbps burst flow to HP queue
 *            when congested flow reaches steady state.
 *            SP scheduling between HP and LP queue.
 */
#include "../helper/star-sim-helper.h"

// #include "ns3/applications-module.h"
// #include "ns3/core-module.h"
// #include "ns3/internet-module.h"
// #include "ns3/network-module.h"
// #include "ns3/point-to-point-module.h"
// #include "ns3/traffic-control-module.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>

using namespace std;

void
split(const string& s, vector<string>& tokens, const string& delimiters)
{
    string::size_type start = s.find_first_not_of(delimiters, 0);
    string::size_type pos = s.find_first_of(delimiters, 0);
    while (pos != string::npos || start != string::npos)
    {
        tokens.emplace_back(s.substr(start, pos - start));
        start = s.find_first_not_of(delimiters, pos);
        pos = s.find_first_of(delimiters, start);
    }
}

using namespace ns3;

namespace ns3
{

namespace hb
{
NS_LOG_COMPONENT_DEFINE("HybridBufferTest");

class StarSimHelperTc202 : public StarSimHelper
{
  public:
    StarSimHelperTc202(std::string simName, Time start = Seconds(0), Time stop = Seconds(1));
    ~StarSimHelperTc202() override;

    void SetupRouterPacketFilter() override;
};

StarSimHelperTc202::StarSimHelperTc202(std::string simName, Time start, Time stop)
    : StarSimHelper(simName, start, stop)
{
    NS_LOG_FUNCTION(this << simName);
}

StarSimHelperTc202::~StarSimHelperTc202()
{
    NS_LOG_FUNCTION(this);
}

void
StarSimHelperTc202::SetupRouterPacketFilter()
{
    NS_LOG_FUNCTION(this);
    uint8_t protocolNumber = IsTcpTransport() ? TcpL4Protocol::PROT_NUMBER : UdpL4Protocol::PROT_NUMBER;
    
    Ptr<TrafficControlLayer> tc = m_hub->GetObject<TrafficControlLayer>();
    // Install packet filters for each output port
    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        Ptr<QueueDisc> rootQdisc = tc->GetRootQueueDiscOnDevice(m_hubDevices.Get(i));
        // root classify rule: one flow goto hp, another goto lp
        Ptr<FiveTuplePacketFilter> rootFilter = CreateObject<FiveTuplePacketFilter>();
        for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
        {
            for (uint32_t cid = 0; cid < m_nReceivers; cid++)
            {
                rootFilter->AddClassifyRule(protocolNumber,
                                            Ipv4Address::GetAny(),
                                            Ipv4Address::GetAny(),
                                            Ipv4Mask::GetZero(),
                                            Ipv4Mask::GetZero(),
                                            0,
                                            0xffff,
                                            0,
                                            0xffff,
                                            0);
            }
        }
        rootQdisc->AddPacketFilter(rootFilter);

        for (uint32_t l2id = 0; l2id < rootQdisc->GetNQueueDiscClasses(); l2id++)
        {
            // layer 2 (hp)
            Ptr<QueueDiscClass> l2Cls = rootQdisc->GetQueueDiscClass(l2id);
            Ptr<QueueDisc> l2Qdisc = l2Cls->GetQueueDisc();
            Ptr<FiveTuplePacketFilter> l2Filter = CreateObject<FiveTuplePacketFilter>();
            for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
            {
                for (uint32_t cid = 0; cid < m_nReceivers; cid++)
                {
                    l2Filter->AddClassifyRule(protocolNumber,
                                              Ipv4Address::GetAny(),
                                            Ipv4Address::GetAny(),
                                            Ipv4Mask::GetZero(),
                                            Ipv4Mask::GetZero(),
                                            0,
                                            0xffff,
                                            0,
                                            0xffff,
                                            0);
                }
            }
            l2Qdisc->AddPacketFilter(l2Filter);
        }
    }
}

} // namespace hb
} // namespace ns3

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    double Deephir_threshold = 0.2;
    uint64_t flow_rate = 100;
    uint64_t if_change_threshold = 0;
    std::string algorithm_name = "BMS";
    std::string transport = "tcp";  // 默认 TCP
    bool enableCustomOutput = false;    //是否打印调试输出  默认是不输出
    std::string trafficGenDir;
    int isWeb = 1;
    int isIncast = 1;
    cmd.AddValue("Deephir_threshold", "deephir阈值", Deephir_threshold);
    cmd.AddValue("if_change_threshold", "是否改变DT阈值", if_change_threshold);
    cmd.AddValue("algorithm_name", "算法名", algorithm_name);
    cmd.AddValue("IsWeb", "真实流量跑Websearch还是hadoop?", isWeb);
    cmd.AddValue("traffic_gen_dir",
             "TrafficGen目录，由run-tests.sh传入",
             trafficGenDir);
    cmd.AddValue("transport","传输协议：tcp 或 udp",
                transport);
    // cmd.AddValue("IsIncast", "真实流量是否加Incast?", isIncast);
    // cmd.AddValue("flow_rate", "流量速率", flow_rate);
    std::cout << "传输协议：" << transport << std::endl;
    std::cout << "是否读取到了" << Deephir_threshold << std::endl;
    cmd.AddValue("enable_custom_output",
             "Enable custom TCP/MMU/RED/P2P output",
             enableCustomOutput);
    cmd.Parse(argc, argv);
    ::setenv("NS3_CUSTOM_OUTPUT",
         enableCustomOutput ? "1" : "0",
         1);
    // CommandLine cmd(__FILE__);
    // cmd.Parse(argc, argv);

    uint32_t numSpokes = 64;   // 8
    uint32_t numReceivers = 12; // 4
    double sim_time = 0.2;
    DataRate recvLinkCapacity = DataRate("100Gbps");
    Time recvLinkDelay = MicroSeconds(0.5);
    DataRate sendLinkCapacity = DataRate("100Gbps"); // 1000Gbps
    Time sendLinkDelay = MicroSeconds(0.5); //100Gbps*80us=1000KB
 

    Config::SetDefault("ns3::SwitchMmu::nextFilePath", StringValue("tc2-03/"));

    Config::SetDefault("ns3::SwitchMmu::now_algorithm_name", StringValue(algorithm_name));
    Config::SetDefault("ns3::SwitchMmu::Deeohir_threshold", DoubleValue(Deephir_threshold));
    Config::SetDefault("ns3::SwitchMmu::if_change_threshold", UintegerValue(1));
    Config::SetDefault("ns3::SwitchMmu::if_test9", UintegerValue(1));
    if (!algorithm_name.compare("pbs"))
    {
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm", EnumValue(2)); // pbs
        Config::SetDefault("ns3::SwitchMmu::now_algorithm_name", StringValue("pbs"));

        std::cout << "yes pbs" << std::endl;
    }
    else
    {
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm", EnumValue(5)); // BMS
        Config::SetDefault("ns3::SwitchMmu::now_algorithm_name", StringValue("BMS"));
    }
    hb::StarSimHelperTc202 simHelper("test-tc2-03", Seconds(0), Seconds(sim_time));
    
    simHelper.SetTransportProtocol(transport);
    simHelper.ConfigTransport();
    simHelper.ConfigTopology(numSpokes,
                             numReceivers,
                             recvLinkCapacity,
                             recvLinkDelay,
                             sendLinkCapacity,
                             sendLinkDelay);
    
    simHelper.ConfigTransport("tcp", "ns3::TcpDctcp");

   
    std::string file;
    if (trafficGenDir.empty())
    {
        std::cerr << "错误：没有收到 traffic_gen_dir 参数" << std::endl;
        return 1;
    }
    std::string filename = trafficGenDir +(isWeb? "/Generated/traffic_web_80.txt" : "/Generated/traffic_fbhdp_80.txt");
    std::cout << "TrafficGen目录：" << trafficGenDir << std::endl;
    std::cout << "读取流量文件：" << filename << std::endl;
    std::ifstream ifile(filename);
    if (!ifile.is_open())
    {
        std::cerr << "无法打开流量文件：" << filename << std::endl;
        return 1;
    }
    // 将文件读入到ostringstream对象buf中
    ostringstream buf;
    char ch;
    while (buf && ifile.get(ch))
        buf.put(ch);
    // 返回与流对象buf关联的字符串
    file = buf.str();
        if (file.empty())
    {
        return 1;
    }
    // 按行分割
    std::vector<std::string> lines;
    split(file, lines, "\n");
        if (lines.empty())
    {
        std::cerr << "错误：流量文件没有可读取的内容："
                << filename
                << std::endl;

        return 1;
    }
    double linkLoad = 0.05; // Incast负载  20%   5%
    double querySize = 0.2 * 4 * 1e6; // 总Incast数据量 40% bufferSize = 1.6MB
    double busrtSize = querySize / (numSpokes-numReceivers);    // 每个发送端每轮发出的数据量 40%*4MB/SenderNums = 1.6MB/6= 266KB    266KB/100Gbps=21.28us
    double intervalperReceiver = querySize / 100 * 8 / linkLoad; // 该端口排空这些数据需要1.6MB/100Gbps=128us， 所以20%的负载要128us/20% = 640us发一轮
    double interval = intervalperReceiver / numReceivers /1e9;  // 单位:s 上述是针对一个端口（接收端），实际上每个接收端都要20%的负载， 那么总体间隔就是要640us/ReceiverNums=640us/6=106.67us
    double nowT = 0.0;
    /*
    5%  1.6MB   1.6MB/12=133KB    133KB/100Gbps=10.6us    1.6MB/100Gbps=128us，所以5%的负载要128us/5% = 2560us发一轮。  12个接收端，那么总体间隔是2560us/12=212us
    */

    // 第一行是流量数量
    //uint32_t dst = 0;
    lines.erase(lines.begin());
    int count = 0;
    for (auto i : lines)    {
        count++;
        std::vector<std::string> words;
        split(i, words, " ");
        for (auto j : words)
            std::cout << j << " ";
            
        if (isIncast){
            while (nowT + interval < std::stod(words.at(2)) - 2)
            {
                // Ptr<ExponentialRandomVariable> sizeVar = CreateObject<ExponentialRandomVariable>();
                // sizeVar->SetAttribute("Mean", DoubleValue(busrtSize));
                uint64_t flowSize = busrtSize; //sizeVar->GetValue();
                uint32_t dst = rand() % numReceivers;
                //if(dst == numReceivers)   dst = 0;
                for (int k = numReceivers; k < numSpokes; k++) 
                    simHelper.AddFlow((uint32_t)k,
                                    dst,
                                    Seconds(nowT),
                                    Seconds(sim_time),
                                    DataRate("100Gbps"),
                                    flowSize);
                nowT += interval;
                //dst++;
            }
        }
        
        simHelper.AddFlow(std::stoi(words.at(0)),
                            std::stoi(words.at(1)),
                            Seconds(std::stod(words.at(2)) - 2),
                            Seconds(sim_time),
                            DataRate("100Gbps"), 
                            std::stoi(words.at(3)));

        cout << "流量大小："
                << std::stod(words.at(3)) / 1000
                << "kB " << std::stoi(words.at(0)) << "到" << std::stoi(words.at(1)) << " start_Time: "
                << Seconds(std::stod(words.at(2)) - 2)  << endl;
    }

    // simHelper.EnableHbmThroughputTracing();
    // simHelper.EnableBufferUsageTracing();
    // simHelper.EnableBmResultTracing();
    // simHelper.EnablePortThroughputTracing();
    // simHelper.EnableQueueThroughputTracing();
    // simHelper.EnableWCacheThroughputTracing();
    // simHelper.EnableSramThroughputTracing();
    // simHelper.EnableQueueWCacheTracing();
    // simHelper.EnableQueueSramTracing();
    // simHelper.EnableQueueHbmTracing();

    simHelper.Run();

    return 0;
}
