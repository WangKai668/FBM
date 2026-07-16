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
 *         n1            n11
 *            \        /
 *  200Gbps,   \      /   100Gbps,
 *  1ms     .   \    /  . 1ms
 *          .     n0    .
 *          .   /    \  .
 *             /      \
 *            /        \
 *         n10           n20
 *
 * - all net devices are reorder-point-to-point net devices
 * - all links are point-to-point links with indicated one-way BW/delay
 * - DropTail queues with backpressure from NetDeviceQueueInterface
 * - Traffic: n1-n10 send 1-to-1 traffic to n11-n20. Burst flows are sent at
 *            110Gbps-200Gbps (in 10Gbps increments) to HP queue.
 *            SP scheduling between HP and LP queue.
 */
#include "../helper/star-sim-helper.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

namespace ns3
{

namespace hb
{
NS_LOG_COMPONENT_DEFINE("HybridBufferTest");

class StarSimHelperTc201 : public StarSimHelper
{
  public:
    StarSimHelperTc201(std::string simName, Time start = Seconds(0), Time stop = Seconds(1));
    ~StarSimHelperTc201() override;

    void SetupRouterPacketFilter() override;
};

StarSimHelperTc201::StarSimHelperTc201(std::string simName, Time start, Time stop)
    : StarSimHelper(simName, start, stop)
{
    NS_LOG_FUNCTION(this << simName);
}

StarSimHelperTc201::~StarSimHelperTc201()
{
    NS_LOG_FUNCTION(this);
}

void
StarSimHelperTc201::SetupRouterPacketFilter()
{
    NS_LOG_FUNCTION(this);
    uint8_t protocolNumber = IsTcpTransport() ? TcpL4Protocol::PROT_NUMBER : UdpL4Protocol::PROT_NUMBER;
    Ptr<TrafficControlLayer> tc = m_hub->GetObject<TrafficControlLayer>();
    // Install packet filters for each output port
    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        std::vector<uint8_t> priCls = {0, 0};
        Ptr<QueueDisc> rootQdisc = tc->GetRootQueueDiscOnDevice(m_hubDevices.Get(i));
        // root classify rule: one flow goto hp, another goto lp
        Ptr<FiveTuplePacketFilter> rootFilter = CreateObject<FiveTuplePacketFilter>();
        // 配置流流向
        for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
        {
            rootFilter->AddClassifyRule(protocolNumber,
                                        m_spokeInterfaces.GetAddress(sid),
                                        m_spokeInterfaces.GetAddress(sid - m_nReceivers),
                                        Ipv4Mask::GetOnes(),
                                        Ipv4Mask::GetOnes(),
                                        0,
                                        0xffff,
                                        0,
                                        0xffff,
                                        priCls[sid - m_nReceivers]);
        }
        rootQdisc->AddPacketFilter(rootFilter);

        for (uint32_t l2id = 0; l2id < rootQdisc->GetNQueueDiscClasses(); l2id++)
        {
            // layer 2 (hp)
            std::vector<uint8_t> qCls = {0, 0};
            Ptr<QueueDiscClass> l2Cls = rootQdisc->GetQueueDiscClass(l2id);
            Ptr<QueueDisc> l2Qdisc = l2Cls->GetQueueDisc();
            Ptr<FiveTuplePacketFilter> l2Filter = CreateObject<FiveTuplePacketFilter>();
            for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
            {
                l2Filter->AddClassifyRule(protocolNumber,
                                          m_spokeInterfaces.GetAddress(sid),
                                          m_spokeInterfaces.GetAddress(sid - m_nReceivers),
                                          Ipv4Mask::GetOnes(),
                                          Ipv4Mask::GetOnes(),
                                          0,
                                          0xffff,
                                          0,
                                          0xffff,
                                          qCls[sid - m_nReceivers]);
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
    // pbs
    CommandLine cmd(__FILE__);
    std::string algorithm_name = "BMS";
    cmd.AddValue("algorithm_name", "算法名", algorithm_name);
    std::string transport = "udp";  // 默认 TCP  这里使用udp
    cmd.AddValue("transport","传输协议：tcp 或 udp",
            transport);
    std::cout << "传输协议：" << transport << std::endl;
    cmd.Parse(argc, argv);

    uint32_t numSpokes = 4;
    uint32_t numReceivers = 2;
    double sim_time = 0.2;
    DataRate recvLinkCapacity = DataRate("100Gbps");
    Time recvLinkDelay = MicroSeconds(1);
    DataRate sendLinkCapacity = DataRate("2000Gbps");
    Time sendLinkDelay = MicroSeconds(1);

    hb::StarSimHelperTc201 simHelper("test-tc2-06", Seconds(0), Seconds(sim_time));
    simHelper.SetTransportProtocol(transport);
    simHelper.ConfigTransport();
    simHelper.ConfigTopology(numSpokes,
                             numReceivers,
                             recvLinkCapacity,
                             recvLinkDelay,
                             sendLinkCapacity,
                             sendLinkDelay);
    simHelper.ConfigTransport("udp");
    uint32_t sendRate1 = 300;
    uint32_t sendRate2 = 900;
    std::string rate1 = std::to_string(sendRate1) + "Gbps";
    std::string rate2 = std::to_string(sendRate2) + "Gbps";

    // 0 1 2 3
    /*
    simHelper.AddFlow(2, 0, Seconds(0.0000), Seconds(0.00045), DataRate(rate1));

    simHelper.AddFlow(2, 0, Seconds(0.0003), Seconds(0.00032), DataRate(rate2));
    simHelper.AddFlow(2, 0, Seconds(0.00035), Seconds(0.00037), DataRate(rate2));
    simHelper.AddFlow(2, 0, Seconds(0.0004), Seconds(0.00042), DataRate(rate2));
    */

    double roundInterval = 0.002; // 每轮间隔2ms
    int rounds = 10;               // 重复次数

    for (int i = 0; i < rounds; ++i)
    {
        double offset = i * roundInterval;

        // 第一个流，速率为 rate1
        simHelper.AddFlow(2, 0, Seconds(offset), Seconds(0.00045 + offset), DataRate(rate1));

        // 第四个流，速率为 rate2
        simHelper.AddFlow(2,
                          0,
                          Seconds(0.00027 + offset),
                          Seconds(0.00030 + offset),
                          DataRate(rate2));

        // 第二个流，速率为 rate2
        simHelper.AddFlow(2,
                          0,
                          Seconds(0.00033 + offset),
                          Seconds(0.00035 + offset),
                          DataRate(rate2));

        // 第三个流，速率为 rate2
        simHelper.AddFlow(2,
                          0,
                          Seconds(0.00038 + offset),
                          Seconds(0.00039 + offset),
                          DataRate(rate2));

                          /*
                          // 第四个流，速率为 rate2
        simHelper.AddFlow(2,
                          0,
                          Seconds(0.00028 + offset),
                          Seconds(0.00030 + offset),
                          DataRate(rate2));

        // 第二个流，速率为 rate2
        simHelper.AddFlow(2,
                          0,
                          Seconds(0.00033 + offset),
                          Seconds(0.00035 + offset),
                          DataRate(rate2));

        // 第三个流，速率为 rate2
        simHelper.AddFlow(2,
                          0,
                          Seconds(0.00038 + offset),
                          Seconds(0.00040 + offset),
                          DataRate(rate2));*/

        
    }

    // simHelper.AddFlow(2, 0, Seconds(0.0020), Seconds(0.0028), DataRate(rate1));

    // simHelper.AddFlow(2, 0, Seconds(0.000080), Seconds(0.000100), DataRate(rate2));
    // simHelper.AddFlow(2, 0, Seconds(0.000105), Seconds(0.000110), DataRate(rate2));
    // simHelper.AddFlow(2, 0, Seconds(0.000115), Seconds(0.000120), DataRate(rate2));
    // simHelper.AddFlow(2, 0, Seconds(0.000270), Seconds(0.000290), DataRate(rate2));
    // simHelper.AddFlow(2, 0, Seconds(0.000295), Seconds(0.000300), DataRate(rate2));
    // simHelper.AddFlow(2, 0, Seconds(0.000305), Seconds(0.000310), DataRate(rate2));

    //  simHelper.AddFlow(2, 0, Seconds(0.000160), Seconds(0.000180), DataRate(rate2));
    //  simHelper.AddFlow(2, 0, Seconds(0.000185), Seconds(0.000190), DataRate(rate2));
    //  simHelper.AddFlow(2, 0, Seconds(0.000195), Seconds(0.000200), DataRate(rate2));

    // simHelper.AddFlow(2, 0, Seconds(0.000105), Seconds(0.000110), DataRate(rate2));

    Config::SetDefault("ns3::SwitchMmu::nextFilePath", StringValue("tc2-06/"));
    Config::SetDefault("ns3::SwitchMmu::now_algorithm_name", StringValue(algorithm_name));
    if (!algorithm_name.compare("pbs"))
    {
        Config::SetDefault("ns3::SwitchMmu::eta_MD", DoubleValue(1)); // pbs
        Config::SetDefault("ns3::SwitchMmu::EWMA_W", DoubleValue(0.1)); // pbs
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm", EnumValue(2)); // pbs
    }
    else
    {
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm", EnumValue(5)); // BMS
    }

    // Config::SetDefault("ns3::SwitchMmu::now_algorithm_name", StringValue("BMS"));
    // Config::SetDefault("ns3::SwitchMmu::BMAlgorithm", EnumValue(5)); // BMS

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
