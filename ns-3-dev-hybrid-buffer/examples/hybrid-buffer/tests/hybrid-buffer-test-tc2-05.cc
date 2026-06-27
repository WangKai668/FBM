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

    uint32_t numSpokes = 22;   // 8
    uint32_t numReceivers = 3; // 4
    double sim_time = 0.2;
    DataRate recvLinkCapacity = DataRate("100Gbps");
    Time recvLinkDelay = MicroSeconds(1);
    DataRate sendLinkCapacity = DataRate("100Gbps"); // 1000Gbps
    Time sendLinkDelay = MicroSeconds(1);

    hb::StarSimHelperTc202 simHelper("test-tc2-05", Seconds(0), Seconds(sim_time));

    simHelper.ConfigTopology(numSpokes,
                             numReceivers,
                             recvLinkCapacity,
                             recvLinkDelay,
                             sendLinkCapacity,
                             sendLinkDelay);

    using namespace std;
    simHelper.AddFlow(3, 0, Seconds(0.0000), Seconds(0.2), DataRate("2000Gbps"), 2000*1000*1000);
    simHelper.AddFlow(4, 0, Seconds(0.0000), Seconds(0.2), DataRate("2000Gbps"), 2000*1000*1000);

    // for (int i = 5; i<=5+9-1; i++){
    //     // simHelper.AddFlow(i, 0, Seconds(0.0000), Seconds(0.0003), DataRate("2000Gbps"));
    //     simHelper.AddFlow(i, 1, Seconds(0.0006), Seconds(0.2), DataRate("2000Gbps"), 500*1000);
    // }
    /*
    
     virtual void AddFlow(uint32_t srcId,
                         uint32_t dstId,
                         Time start,
                         Time stop,
                         DataRate rate = 0,
                         uint64_t flowSize = 0,
                         std::string onTime = "ns3::ConstantRandomVariable[Constant=1000]",
                         std::string offTime = "ns3::ConstantRandomVariable[Constant=0]",
                         uint32_t pktSize = 0)
    {
        FlowInfo flow = {.srcId = srcId,
                         .dstId = dstId,
                         .startTime = start,
                         .stopTime = stop,
                         .rate = rate,
                         .size = flowSize,
                         .onTime = onTime,
                         .offTime = offTime,
                         .pktSize = pktSize};
    */
    

    Config::SetDefault("ns3::SwitchMmu::nextFilePath", StringValue("tc2-05/"));
    Config::SetDefault("ns3::SwitchMmu::now_algorithm_name", StringValue(algorithm_name));
    Config::SetDefault("ns3::SwitchMmu::Deeohir_threshold", DoubleValue(Deephir_threshold));
    Config::SetDefault("ns3::SwitchMmu::if_change_threshold", UintegerValue(0));
    Config::SetDefault("ns3::SwitchMmu::if_test9", UintegerValue(0));
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

    simHelper.EnableHbmThroughputTracing();
    simHelper.EnableBufferUsageTracing();
    simHelper.EnableBmResultTracing();
    simHelper.EnablePortThroughputTracing();
    simHelper.EnableQueueThroughputTracing();
    simHelper.EnableWCacheThroughputTracing();
    simHelper.EnableSramThroughputTracing();
    simHelper.EnableQueueWCacheTracing();
    simHelper.EnableQueueSramTracing();
    simHelper.EnableQueueHbmTracing();

    simHelper.Run();

    return 0;
}
