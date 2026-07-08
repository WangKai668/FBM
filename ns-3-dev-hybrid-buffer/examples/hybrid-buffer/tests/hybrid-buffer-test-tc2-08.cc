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
 *         n1            n6
 *            \        /
 *  240Gbps,   \      /   100Gbps,
 *  1ms     .   \    /  . 1ms
 *          .     n0    .
 *          .   /    \  .
 *  240Gbps,   /      \
 *  1ms       /        \
 *         n5            n10
 *
 * - all net devices are reorder-point-to-point net devices
 * - all links are point-to-point links with indicated one-way BW/delay
 * - DropTail queues with backpressure from NetDeviceQueueInterface
 * - Traffic: n1-n5 send 1-to-1 traffic to n6-n10. Traffic from n2-n5 is 120Gbps
 *            burst flow to HP queue, n1 starts 120Gbps burst flow to LP queue
 *            when former flow reaches steady state.
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
            rootFilter->AddClassifyRule(UdpL4Protocol::PROT_NUMBER,
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
                l2Filter->AddClassifyRule(UdpL4Protocol::PROT_NUMBER,
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
    double Deephir_threshold = 0.2;
    uint64_t flow_rate = 100;
    uint64_t if_change_threshold = 0;
    std::string algorithm_name = "BMS";
    cmd.AddValue("Deephir_threshold", "deephir阈值", Deephir_threshold);
    cmd.AddValue("if_change_threshold", "是否改变DT阈值", if_change_threshold);
    cmd.AddValue("algorithm_name", "算法名", algorithm_name);
    cmd.AddValue("flow_rate", "流量速率", flow_rate);

    std::cout << "是否读取到了" << Deephir_threshold << std::endl;
    cmd.Parse(argc, argv);
    Config::SetDefault("ns3::SwitchMmu::nextFilePath",
                    StringValue(""));
    Config::SetDefault("ns3::SwitchMmu::now_algorithm_name", StringValue(algorithm_name));
    Config::SetDefault("ns3::SwitchMmu::Deeohir_threshold", DoubleValue(Deephir_threshold));
    Config::SetDefault("ns3::SwitchMmu::flow_rate", UintegerValue(flow_rate));
    Config::SetDefault("ns3::SwitchMmu::if_change_threshold", UintegerValue(1));
    Config::SetDefault("ns3::SwitchMmu::if_test8",
                    UintegerValue(0));
    if (!algorithm_name.compare("pbs"))
    {
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm", EnumValue(2)); // pbs
    }
    else
    {
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm", EnumValue(5)); // BMS
    }

    uint32_t numSpokes = 4;
    uint32_t numReceivers = 2;
    double sim_time = 0.2;
    DataRate recvLinkCapacity = DataRate("100Gbps");
    Time recvLinkDelay = MicroSeconds(1);
    DataRate sendLinkCapacity = DataRate("1000Gbps");
    Time sendLinkDelay = MicroSeconds(1);

    hb::StarSimHelperTc201 simHelper("test-tc2-08", Seconds(0), Seconds(sim_time));

    simHelper.ConfigTopology(numSpokes,
                             numReceivers,
                             recvLinkCapacity,
                             recvLinkDelay,
                             sendLinkCapacity,
                             sendLinkDelay);

    uint32_t sendRate1 = 500; // 25000MB/s
    uint32_t sendRate2 = flow_rate;
    double flow_size = 3.5; // MB
    double lastTime = flow_size / (static_cast<double>(flow_rate) / 8.0 * 1000.0);
    std::string rate1 = std::to_string(sendRate1) + "Gbps";
    std::string rate2 = std::to_string(sendRate2) + "Gbps";

    // 0 1 2 3 4 5
    // 0.00012 3M
    // 0.000008 0.2M
    // 0.00004 1M
    // 0.00008 2M
    // 0.00012 3M
    // 0.00016 4M 0 1 2 3
    simHelper.AddFlow(3, 1, Seconds(0.0000), Seconds(0.0009), DataRate(rate1));
    // simHelper.AddFlow(3, 0, Seconds(0.0000), Seconds(0.0005), DataRate(rate1));
    // simHelper.AddFlow(4, 1, Seconds(0.000008), Seconds(0.000008 + lastTime), DataRate(rate2));
    simHelper.AddFlow(2, 0, Seconds(0.0006), Seconds(0.0006 + lastTime), DataRate(rate2));
    // simHelper.AddFlow(5, 2, Seconds(0.0000), Seconds(0.0005), DataRate(rate1));
    // simHelper.AddFlow(3, 0, Seconds(0.0000), Seconds(0.0005), DataRate(rate1));
    // // simHelper.AddFlow(4, 1, Seconds(0.000008), Seconds(0.000008 + lastTime), DataRate(rate2));
    // simHelper.AddFlow(4, 1, Seconds(0.00021), Seconds(0.00021 + lastTime), DataRate(rate2));
    // simHelper.AddFlow(4, 1, Seconds(0.0001), Seconds(0.00011 + lastTime),
    // DataRate(rate2));0.0000342857
    std::cout << "lastTime:" << lastTime << std::endl;
    std::cout << "endTime:" << 0.00011 + lastTime << std::endl;


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
