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
    Ptr<TrafficControlLayer> tc = m_hub->GetObject<TrafficControlLayer>();
    // Install packet filters for each output port
    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        std::vector<uint8_t> priCls =
            {0, 0, 0, 0, 0, 0}; // 之前是4个，现在是6个/////////////////////////////////////////
        Ptr<QueueDisc> rootQdisc = tc->GetRootQueueDiscOnDevice(m_hubDevices.Get(i));
        // root classify rule: one flow goto hp, another goto lp
        Ptr<FiveTuplePacketFilter> rootFilter = CreateObject<FiveTuplePacketFilter>();
        for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
        {
            for (uint32_t cid = 0; cid < m_nReceivers; cid++)
            {
                rootFilter->AddClassifyRule(UdpL4Protocol::PROT_NUMBER,
                                            m_spokeInterfaces.GetAddress(sid),
                                            m_spokeInterfaces.GetAddress(cid),
                                            Ipv4Mask::GetOnes(),
                                            Ipv4Mask::GetOnes(),
                                            0,
                                            0xffff,
                                            0,
                                            0xffff,
                                            priCls[sid - m_nReceivers]);
            }
        }
        rootQdisc->AddPacketFilter(rootFilter);

        for (uint32_t l2id = 0; l2id < rootQdisc->GetNQueueDiscClasses(); l2id++)
        {
            // layer 2 (hp)
            std::vector<uint8_t> qCls =
                {0, 0, 0, 0, 0, 0}; // 同上////////////////////////////////////////////////
            Ptr<QueueDiscClass> l2Cls = rootQdisc->GetQueueDiscClass(l2id);
            Ptr<QueueDisc> l2Qdisc = l2Cls->GetQueueDisc();
            Ptr<FiveTuplePacketFilter> l2Filter = CreateObject<FiveTuplePacketFilter>();
            for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
            {
                for (uint32_t cid = 0; cid < m_nReceivers; cid++)
                {
                    l2Filter->AddClassifyRule(UdpL4Protocol::PROT_NUMBER,
                                              m_spokeInterfaces.GetAddress(sid),
                                              m_spokeInterfaces.GetAddress(cid),
                                              Ipv4Mask::GetOnes(),
                                              Ipv4Mask::GetOnes(),
                                              0,
                                              0xffff,
                                              0,
                                              0xffff,
                                              qCls[sid - m_nReceivers]);
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
    std::string trafficGenDir;
    int isWeb = 1;
    int isIncast = 1;
    cmd.AddValue("Deephir_threshold", "deephir阈值", Deephir_threshold);
    cmd.AddValue("if_change_threshold", "是否改变DT阈值", if_change_threshold);
    cmd.AddValue("algorithm_name", "算法名", algorithm_name);
    cmd.AddValue("IsWeb", "真实流量跑Websearch还是hadoop?", isWeb);
    cmd.AddValue("IsIncast", "真实流量是否加Incast?", isIncast);
    cmd.AddValue("traffic_gen_dir",
             "TrafficGen目录，由run-tests.sh传入",
             trafficGenDir);
    // cmd.AddValue("flow_rate", "流量速率", flow_rate);

    std::cout << "是否读取到了" << Deephir_threshold << std::endl;
    cmd.Parse(argc, argv);

    // CommandLine cmd(__FILE__);
    // cmd.Parse(argc, argv);

    uint32_t numSpokes = 12;   // 8
    uint32_t numReceivers = 6; // 4
    double sim_time = 0.2;
    DataRate recvLinkCapacity = DataRate("100Gbps");
    Time recvLinkDelay = MicroSeconds(1);
    DataRate sendLinkCapacity = DataRate("5000Gbps"); // 1000Gbps
    Time sendLinkDelay = MicroSeconds(1);

    hb::StarSimHelperTc202 simHelper("test-tc2-11", Seconds(0), Seconds(sim_time));

    simHelper.ConfigTopology(numSpokes,
                             numReceivers,
                             recvLinkCapacity,
                             recvLinkDelay,
                             sendLinkCapacity,
                             sendLinkDelay);

    uint32_t sendRate1 = 180;
    uint32_t sendRate2 = 900;
    std::string rate1 = std::to_string(sendRate1) + "Gbps";
    std::string rate2 = std::to_string(sendRate2) + "Gbps";

    double interval = 0.0010;
    double lastTime = 0.0005;
    double nowT = 0.0;

    // /*原来的*/{
    //     for (uint32_t sendId = numReceivers+1; sendId < numSpokes; sendId++)
    //     {
    //         simHelper.AddFlow(sendId, sendId-numReceivers, Seconds(0.1), Seconds(0.1003),
    //         DataRate(rate1));
    //     }
    //     simHelper.AddFlow(5, 0, Seconds(0.1002), Seconds(0.10022), DataRate(rate2));
    // }

    /*后来的*/ {
        std::string file;

        if (trafficGenDir.empty())
        {
            std::cerr << "错误：没有收到 traffic_gen_dir 参数" << std::endl;
            return 1;
        }

        std::string filename = trafficGenDir +  (isWeb? "/Generated/traffic_web.txt" : "/Generated/traffic_fbhdp.txt");
        std::cout << "TrafficGen目录：" << trafficGenDir << std::endl;
        std::cout << "读取流量文件：" << filename << std::endl;
        
        {
            using namespace std;

            // 从文件读入到string里
            ifstream ifile(filename);
            // 将文件读入到ostringstream对象buf中
            ostringstream buf;
            char ch;
            while (buf && ifile.get(ch))
                buf.put(ch);
            // 返回与流对象buf关联的字符串
            file = buf.str();
        }
        std::vector<std::string> lines;
        split(file, lines, "\n");
        lines.erase(lines.begin());
        int count = 0;
        for (auto i : lines)
        {
            count++;
            std::vector<std::string> words;
            split(i, words, " ");
            for (auto j : words)
                std::cout << j << " ";
            std::cout << std::endl;
            try
            {
                if (isIncast){
                    while (nowT + interval < std::stod(words.at(2)) - 2)
                    {
                        // for (int k = numSpokes-5; k < numSpokes; k++) 
                        //     simHelper.AddFlow((uint32_t)k,
                        //                     0,
                        //                     Seconds(nowT),
                        //                     Seconds(nowT + lastTime),
                        //                     DataRate("100Gbps"));

                        simHelper.AddFlow(6,
                                        0,
                                        Seconds(nowT),
                                        Seconds(nowT + lastTime),
                                        DataRate("300Gbps"));
                        nowT += interval;
                        cout << "流量大小Incast：" << (lastTime) * 300 * 1e9 / 8 / 1e6 << "kB " << 6
                            << "到" << 0 << " " << Seconds(nowT) << "到" << Seconds(nowT + lastTime)
                            << " " << DataRate(words.at(4)) << endl;
                    }
                }

                simHelper.AddFlow(std::stoi(words.at(0)),
                                  std::stoi(words.at(1)),
                                  Seconds(std::stod(words.at(2)) - 2),
                                  Seconds(std::stod(words.at(3)) - 2),
                                  DataRate(words.at(4)));

                cout << "流量大小："
                     << (std::stod(words.at(3)) - std::stod(words.at(2))) *
                            std::stod(words.at(4).substr(0, words.at(4).find("Gbps"))) * 1000000000 / 8 /
                            1000
                     << "kB " << std::stoi(words.at(0)) << "到" << std::stoi(words.at(1)) << " "
                     << Seconds(std::stod(words.at(2)) - 2) << "到"
                     << Seconds(std::stod(words.at(3)) - 2) << " " << DataRate(words.at(4)) << endl;
                
                flow_rate = std::stoi(words.at(4).substr(0, words.at(4).find("Gbps")));
            }
            catch (exception e)
            {
                cout << "E" << count << endl;
            }
        }
    }

    Config::SetDefault("ns3::SwitchMmu::nextFilePath", StringValue("tc2-11/"));

    Config::SetDefault("ns3::SwitchMmu::now_algorithm_name", StringValue(algorithm_name));
    Config::SetDefault("ns3::SwitchMmu::Deeohir_threshold", DoubleValue(Deephir_threshold));
    Config::SetDefault("ns3::SwitchMmu::flow_rate", UintegerValue(flow_rate));
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
