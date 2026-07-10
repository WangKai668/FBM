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
 * Network topology (star)
 *
 *                     +--> receiver n0 (100 Gbps)
 * sender n2 (100 Gbps)|
 * sender n3 (100 Gbps)+--> 2-to-1 persistent TCP congestion on output port 0
 *
 * sender n4 ... n13 (each 100 Gbps)
 *                     +--> receiver n1 (100 Gbps)
 *                          10-to-1 TCP burst on output port 1
 *                          one burst every 5 ms
 *                          250 KB per sender, 2.5 MB per burst round
 *
 * One-way link delay: 20 us.
 * Transport: TCP DCTCP.
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
        Ptr<QueueDisc> rootQdisc = tc->GetRootQueueDiscOnDevice(m_hubDevices.Get(i));
        // root classify rule: one flow goto hp, another goto lp
        Ptr<FiveTuplePacketFilter> rootFilter = CreateObject<FiveTuplePacketFilter>();
        for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
        {
            for (uint32_t cid = 0; cid < m_nReceivers; cid++)
            {
                rootFilter->AddClassifyRule(TcpL4Protocol::PROT_NUMBER,
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
                    l2Filter->AddClassifyRule(TcpL4Protocol::PROT_NUMBER,
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
    uint64_t if_change_threshold = 0;
    std::string algorithm_name = "BMS";
    cmd.AddValue("Deephir_threshold", "deephir阈值", Deephir_threshold);
    cmd.AddValue("if_change_threshold", "是否改变DT阈值", if_change_threshold);
    cmd.AddValue("algorithm_name", "算法名", algorithm_name);
    // cmd.AddValue("IsWeb", "真实流量跑Websearch还是hadoop?", isWeb);

    cmd.Parse(argc, argv);


    // 2 个接收端口 + 2 个持续流发送端 + 10 个突发流发送端
    // 接收端：节点 0、1
    // 发送端：节点 2～13
    uint32_t numSpokes = 14;
    uint32_t numReceivers = 2;
    double sim_time = 0.026;

    DataRate recvLinkCapacity = DataRate("100Gbps");
    Time recvLinkDelay = MicroSeconds(20);

    DataRate sendLinkCapacity = DataRate("100Gbps");
    Time sendLinkDelay = MicroSeconds(20);

    Config::SetDefault("ns3::SwitchMmu::nextFilePath",
                    StringValue("tc2-05/"));

    Config::SetDefault("ns3::SwitchMmu::now_algorithm_name",
                    StringValue(algorithm_name));

    Config::SetDefault("ns3::SwitchMmu::Deeohir_threshold",
                    DoubleValue(Deephir_threshold));

    Config::SetDefault("ns3::SwitchMmu::if_change_threshold",
                    UintegerValue(if_change_threshold));

    Config::SetDefault("ns3::SwitchMmu::if_test9",
                    UintegerValue(0));

    if (algorithm_name == "pbs")
    {
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm",
                        EnumValue(2));

        Config::SetDefault("ns3::SwitchMmu::now_algorithm_name",
                        StringValue("pbs"));
    }
    else
    {
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm",
                        EnumValue(5));

        Config::SetDefault("ns3::SwitchMmu::now_algorithm_name",
                        StringValue("BMS"));
    }

    hb::StarSimHelperTc202 simHelper("test-tc2-05",
                                    Seconds(0),
                                    Seconds(sim_time));

    simHelper.ConfigTopology(numSpokes,
                            numReceivers,
                            recvLinkCapacity,
                            recvLinkDelay,
                            sendLinkCapacity,
                            sendLinkDelay);

    // 使用TCP DCTCP，使持续拥塞流达到相对稳定状态
    simHelper.ConfigTransport("tcp", "ns3::TcpDctcp");

    // ---------------------------------------------------------
    // 第一部分：端口 0 上形成 2-to-1 持续拥塞
    // 节点 2、3 持续向接收端 0 发送
    // ---------------------------------------------------------

    const uint64_t persistentFlowSize =
        1000ULL * 1000 * 1000; // 1 GB，保证26ms内不会结束

    simHelper.AddFlow(2,
                    0,
                    Seconds(0.0),
                    Seconds(sim_time),
                    DataRate("100Gbps"),
                    persistentFlowSize);

    simHelper.AddFlow(3,
                    0,
                    Seconds(0.0),
                    Seconds(sim_time),
                    DataRate("100Gbps"),
                    persistentFlowSize);

    // ---------------------------------------------------------
    // 第二部分：端口 1 上每隔 5 ms 注入一次 10-to-1 突发
    // 节点 4～13 同时向接收端 1 发送
    // 每个发送端 250 KB，每轮总量 250 KB × 10 = 2.5 MB
    // ---------------------------------------------------------

    const uint64_t burstFlowSize = 250ULL * 1000;
    const double burstTimes[] = {
        0.005,
        0.010,
        0.015,
        0.020
    };

    for (double burstTime : burstTimes)
    {
        for (uint32_t src = 4; src <= 13; ++src)
        {
            simHelper.AddFlow(src,
                            1,
                            Seconds(burstTime),
                            Seconds(sim_time),
                            DataRate("100Gbps"),
                            burstFlowSize);
        }
    }
    simHelper.EnableHbmThroughputTracing();
    simHelper.EnableBufferUsageTracing();
    simHelper.EnableBmResultTracing();
    simHelper.EnablePortThroughputTracing(MicroSeconds(10));
    simHelper.EnableQueueThroughputTracing(MicroSeconds(10));
    simHelper.EnableWCacheThroughputTracing();
    simHelper.EnableSramThroughputTracing();
    simHelper.EnableQueueWCacheTracing();
    simHelper.EnableQueueSramTracing();
    simHelper.EnableQueueHbmTracing();

    simHelper.Run();

    return 0;
}