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
 *  n1
 *     \ 120 Gb/s, 1ms
 *      \          100Gb/s, 1ms
 *       n0 -------------------------n3
 *      /
 *     / 120 Gb/s, 1ms
 *  n2
 *
 * - all net devices are reorder-point-to-point net devices
 * - all links are point-to-point links with indicated one-way BW/delay
 * - DropTail queues with backpressure from NetDeviceQueueInterface
 * - Traffic: 120Gbps burst flow from n1 to n3, HP queue.
 *            120Gbps burst flow from n2 to n3, LP queue.
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

class StarSimHelperTc103 : public StarSimHelper
{
  public:
    StarSimHelperTc103(std::string simName, Time start = Seconds(0), Time stop = Seconds(1));
    ~StarSimHelperTc103() override;

    void SetupRouterPacketFilter() override;
};

StarSimHelperTc103::StarSimHelperTc103(std::string simName, Time start, Time stop)
    : StarSimHelper(simName, start, stop)
{
    NS_LOG_FUNCTION(this << simName);
}

StarSimHelperTc103::~StarSimHelperTc103()
{
    NS_LOG_FUNCTION(this);
}

void
StarSimHelperTc103::SetupRouterPacketFilter()
{
    NS_LOG_FUNCTION(this);
    Ptr<TrafficControlLayer> tc = m_hub->GetObject<TrafficControlLayer>();
    // Install packet filters for each output port
    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        std::vector<uint8_t> priCls = {0,0};//0表示高优先级，1表示低优先级

        Ptr<QueueDisc> rootQdisc = tc->GetRootQueueDiscOnDevice(m_hubDevices.Get(i));
        // root classify rule: one flow goto hp, another goto lp
        Ptr<FiveTuplePacketFilter> rootFilter = CreateObject<FiveTuplePacketFilter>();
        for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
        {
            rootFilter->AddClassifyRule(UdpL4Protocol::PROT_NUMBER,
                                        m_spokeInterfaces.GetAddress(sid),
                                        m_spokeInterfaces.GetAddress(0),
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
            std::vector<uint8_t> qCls = {1,0};//前为低优先级队列索引号，后为高优先级队列索引号
            Ptr<QueueDiscClass> l2Cls = rootQdisc->GetQueueDiscClass(l2id);
            Ptr<QueueDisc> l2Qdisc = l2Cls->GetQueueDisc();
            Ptr<FiveTuplePacketFilter> l2Filter = CreateObject<FiveTuplePacketFilter>();
            for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
            {
                l2Filter->AddClassifyRule(UdpL4Protocol::PROT_NUMBER,
                                          m_spokeInterfaces.GetAddress(sid),
                                          m_spokeInterfaces.GetAddress(0),
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
    //  //std::cout<<"sjkks"<<std::endl;
    //  LogComponentEnable("HybridBufferTest", LOG_LEVEL_ALL);
    // // LogComponentEnable("SimHelper", LOG_LEVEL_ALL);
    // // LogComponentEnable("StarSimHelper", LOG_LEVEL_ALL);
    //  LogComponentEnable("SwitchMmu", LOG_LEVEL_ALL);
    //  LogComponentEnable("OffChipBuffer", LOG_LEVEL_ALL);
    // // LogComponentEnable("PointToPointReorderNetDevice", LOG_LEVEL_ALL);
    //  //LogComponentEnable("PointToPointReorderHelper", LOG_LEVEL_ALL);
    // // LogComponentEnable("PrioQueueDisc", ns3::LOG_LEVEL_ALL);
    // // LogComponentEnable("DrrQueueDisc", LOG_LEVEL_ALL);
    //  //LogComponentEnable("FifoQueueDisc", LOG_LEVEL_ALL);
    //  //LogComponentEnable("QueueDisc", LOG_LEVEL_ALL);
    //  //LogComponentEnable("FiveTuplePacketFilter", LOG_LEVEL_ALL);
    //  //LogComponentEnable("TrafficControlLayer", LOG_LEVEL_ALL);
    //  //LogComponentEnable("PacketSink", LOG_LEVEL_ALL);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    uint32_t numSpokes = 3;
    uint32_t numReceivers = 1;
    double sim_time=0.2;
    DataRate recvLinkCapacity = DataRate("100Gbps");
    Time recvLinkDelay = MicroSeconds(1);
    DataRate sendLinkCapacity = DataRate("1000Gbps");
    Time sendLinkDelay = MicroSeconds(1);

    hb::StarSimHelperTc103 simHelper("test-tc1-01", Seconds(0), Seconds(sim_time));

    simHelper.ConfigTopology(numSpokes,
                             numReceivers,
                             recvLinkCapacity,
                             recvLinkDelay,
                             sendLinkCapacity,
                             sendLinkDelay);
    
    uint32_t sendRate1 = 400;
    uint32_t sendRate2 = 800;
    std::string rate1 = std::to_string(sendRate1) + "Gbps";
    std::string rate2 = std::to_string(sendRate2) + "Gbps";

    // for (uint32_t sendId = 1; sendId < numSpokes; sendId++)
    // {
    //     simHelper.AddFlow(sendId, 0, Seconds(0.1), Seconds(0.10007), DataRate(rate1));
    //     simHelper.AddFlow(sendId, 0, Seconds(0.10005), Seconds(0.1000695), DataRate(rate2));
    // }

    simHelper.AddFlow(1, 0, Seconds(0.1), Seconds(0.1002), DataRate(rate1));
    simHelper.AddFlow(2, 0, Seconds(0.1), Seconds(0.10002), DataRate(rate2));
    simHelper.AddFlow(2, 0, Seconds(0.10013), Seconds(0.10015), DataRate(rate2));

    Config::SetDefault("ns3::SwitchMmu::Simlulator_time_stop", DoubleValue(0.1002-0.1));
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
